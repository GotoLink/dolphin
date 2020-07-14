// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Debugger/CodeTraceDialog.h"

#include <chrono>
#include <optional>
#include <regex>
#include <vector>

#include <fmt/format.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSpacerItem>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common/Debug/CodeTrace.h"
#include "Common/Event.h"
#include "Common/StringUtil.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinQt/Host.h"
#include "DolphinQt/Settings.h"

#include "DolphinQt/Debugger/CodeWidget.h"

constexpr int ADDRESS_ROLE = Qt::UserRole;
constexpr int MEM_ADDRESS_ROLE = Qt::UserRole + 1;

CodeTraceDialog::CodeTraceDialog(CodeWidget* parent) : QDialog(parent), m_parent(parent)
{
  setWindowTitle(tr("Trace"));
  CreateWidgets();
  ConnectWidgets();
  UpdateBreakpoints();
}

void CodeTraceDialog::reject()
{
  // Make sure to free memory and reset info message.
  ClearAll();
  auto& settings = Settings::GetQSettings();
  settings.setValue(QStringLiteral("tracedialog/geometry"), saveGeometry());
  QDialog::reject();
}

void CodeTraceDialog::CreateWidgets()
{
  auto& settings = Settings::GetQSettings();
  restoreGeometry(settings.value(QStringLiteral("tracedialog/geometry")).toByteArray());
  auto* input_layout = new QHBoxLayout;
  m_trace_target = new QLineEdit();
  m_trace_target->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_trace_target->setPlaceholderText(tr("Register or Memory"));
  m_bp1 = new QComboBox();
  m_bp1->setEditable(true);
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  m_bp1->setCurrentText(tr("Uses PC as trace starting point."));
  m_bp1->setDisabled(true);
  m_bp2 = new QComboBox();
  m_bp2->setEditable(true);
  m_bp2->setCurrentText(tr("Stop BP or address"));

  input_layout->addWidget(m_trace_target);
  input_layout->addWidget(m_bp1);
  input_layout->addWidget(m_bp2);

  auto* boxes_layout = new QHBoxLayout;
  m_backtrace = new QCheckBox(tr("Backtrace"));
  m_verbose = new QCheckBox(tr("Verbose"));
  m_clear_on_loop = new QCheckBox(tr("Reset on loopback"));
  m_record_limit_label = new QLabel(tr("Maximum to record"));
  m_reprocess = new QPushButton(tr("Track Target"));
  m_record_limit_input = new QSpinBox();
  m_record_limit_input->setMinimum(1000);
  m_record_limit_input->setMaximum(200000);
  m_record_limit_input->setValue(10000);
  m_record_limit_input->setSingleStep(10000);
  m_record_limit_input->setMinimumSize(70, 0);
  m_results_limit_label = new QLabel(tr("Maximum results"));
  m_results_limit_input = new QSpinBox();
  m_results_limit_input->setMinimum(100);
  m_results_limit_input->setMaximum(10000);
  m_results_limit_input->setValue(1000);
  m_results_limit_input->setSingleStep(250);
  m_results_limit_input->setMinimumSize(50, 0);

  auto* record_layout = new QHBoxLayout;
  m_record_trace = new QPushButton(tr("Record Trace"));
  m_record_trace->setCheckable(true);
  m_change_range = new QCheckBox(tr("Change Range"));
  m_change_range->setDisabled(true);

  boxes_layout->addWidget(m_reprocess);
  boxes_layout->addWidget(m_backtrace);
  boxes_layout->addWidget(m_verbose);
  boxes_layout->addWidget(m_change_range);
  boxes_layout->addWidget(m_results_limit_label);
  boxes_layout->addWidget(m_results_limit_input);
  boxes_layout->addItem(new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Maximum));
  boxes_layout->addWidget(m_record_limit_label);
  boxes_layout->addWidget(m_record_limit_input);
  boxes_layout->addWidget(m_clear_on_loop);
  boxes_layout->addWidget(m_record_trace);

  m_output_list = new QListWidget();
  m_output_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // QFont font(QStringLiteral("Monospace"));
  // font.setStyleHint(QFont::TypeWriter);
  QFont fixedfont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  fixedfont.setPointSize(11);
  m_output_list->setFont(fixedfont);
  m_output_list->setContextMenuPolicy(Qt::CustomContextMenu);

  auto* actions_layout = new QHBoxLayout();
  actions_layout->addLayout(boxes_layout);
  actions_layout->addItem(new QSpacerItem(1000, 0, QSizePolicy::Expanding, QSizePolicy::Maximum));
  actions_layout->addLayout(record_layout);

  auto* layout = new QVBoxLayout();
  layout->addLayout(input_layout);
  layout->addLayout(actions_layout);
  layout->addWidget(m_output_list);

  InfoDisp();

  setLayout(layout);
}

void CodeTraceDialog::ConnectWidgets()
{
  connect(m_parent, &CodeWidget::BreakpointsChanged, this, &CodeTraceDialog::UpdateBreakpoints);
  connect(m_record_trace, &QPushButton::clicked, [this](bool record) {
    if (record)
      OnRecordTrace(record);
    else
      ClearAll();
  });
  connect(m_reprocess, &QPushButton::pressed, this, &CodeTraceDialog::DisplayTrace);
  connect(m_change_range, &QCheckBox::toggled, this, &CodeTraceDialog::OnChangeRange);
  connect(m_output_list, &QListWidget::itemClicked, m_parent, [this](QListWidgetItem* item) {
    m_parent->SetAddress(item->data(ADDRESS_ROLE).toUInt(),
                         CodeViewWidget::SetAddressUpdate::WithUpdate);
  });
  connect(m_output_list, &CodeTraceDialog::customContextMenuRequested, this,
          &CodeTraceDialog::OnContextMenu);
}

void CodeTraceDialog::ClearAll()
{
  std::vector<TraceOutput>().swap(m_code_trace);
  m_output_list->clear();
  m_bp1->setDisabled(true);
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  m_bp1->setCurrentText(tr("Uses PC as trace starting point."));
  m_bp2->setEnabled(true);
  m_change_range->setChecked(false);
  m_change_range->setDisabled(true);
  m_record_trace->setText(tr("Record Trace"));
  m_record_trace->setChecked(false);
  m_record_limit_label->setText(tr("Maximum to record"));
  m_results_limit_label->setText(tr("Maximum results"));
  UpdateBreakpoints();
  InfoDisp();
}

void CodeTraceDialog::OnRecordTrace(bool checked)
{
  m_record_trace->setChecked(false);

  if (!CPU::IsStepping() || m_recording)
    return;

  // Try to get end_bp based on editable input text, then on combo box selection.
  bool good;
  u32 start_bp = PC;
  u32 end_bp = m_bp2->currentText().toUInt(&good, 16);
  if (!good)
    end_bp = m_bp2->currentData().toUInt(&good);
  if (!good)
    return;

  m_recording = true;
  m_record_trace->setDisabled(true);
  m_reprocess->setDisabled(true);

  m_record_limit = m_record_limit_input->value();

  bool timed_out =
      CT.RecordCodeTrace(&m_code_trace, m_record_limit, 10, end_bp, m_clear_on_loop->isChecked());

  // UpdateDisasmDialog causes a crash now. We're using CPU::StepOpcode(&sync_event) which doesn't
  // appear to require an UpdateDisasmDialog anyway.
  // emit Host::GetInstance()->UpdateDisasmDialog();

  // Errors
  m_error_msg.clear();

  if (timed_out && m_code_trace.empty())
    new QListWidgetItem(tr("Record failed to run."), m_output_list);
  else if (timed_out)
    m_error_msg = tr("Record trace ran out of time. Backtrace won't be correct.");

  // Record actual start and end into combo boxes.
  m_bp1->setDisabled(false);
  m_bp1->clear();
  QString instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(start_bp));
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
  m_bp1->addItem(
      QStringLiteral("Trace Begin   %1 : %2").arg(start_bp, 8, 16, QLatin1Char('0')).arg(instr),
      start_bp);
  m_bp1->setDisabled(true);

  instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(PC - 4));
  instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
  m_bp2->insertItem(
      0, QStringLiteral("Trace End   %1 : %2").arg((PC - 4), 8, 16, QLatin1Char('0')).arg(instr),
      (PC - 4));
  m_bp2->setCurrentIndex(0);
  m_bp2->setDisabled(true);

  // Update UI
  m_change_range->setEnabled(true);
  m_record_trace->setDisabled(false);
  m_reprocess->setDisabled(false);
  m_recording = false;
  m_record_trace->setChecked(true);
  m_record_trace->setText(tr("Reset All"));

  CodeTraceDialog::DisplayTrace();
}

std::vector<TraceOutput> CodeTraceDialog::CodePath(u32 start, u32 end, u32 results_limit)
{
  // Shows entire trace without filtering if target input is blank.
  std::vector<TraceOutput> tmp_out;
  auto begin_itr = m_code_trace.begin();
  auto end_itr = m_code_trace.end();
  size_t trace_size = m_code_trace.size();

  if (m_change_range->isChecked())
  {
    auto begin_itr_temp = find_if(m_code_trace.begin(), m_code_trace.end(),
                                  [start](const TraceOutput& t) { return t.address == start; });
    auto end_itr_temp =
        find_if(m_code_trace.rbegin(), m_code_trace.rend(), [end](const TraceOutput& t) {
          return t.address == end;
        }).base();

    if (begin_itr_temp == m_code_trace.end() || end_itr_temp == m_code_trace.begin())
    {
      new QListWidgetItem(tr("Change Range using invalid addresses. Using full range."),
                          m_output_list);
    }
    else
    {
      begin_itr = begin_itr_temp;
      end_itr = end_itr_temp;
      trace_size = std::distance(begin_itr, end_itr);
    }
  }

  if (m_backtrace->isChecked())
  {
    auto rend_itr = std::reverse_iterator(begin_itr);
    auto rbegin_itr = std::reverse_iterator(end_itr);

    if (results_limit < trace_size)
      rend_itr = std::next(rbegin_itr, results_limit);

    return std::vector<TraceOutput>(rbegin_itr, rend_itr);
  }
  else
  {
    if (results_limit < trace_size)
      end_itr = std::next(begin_itr, results_limit);

    return std::vector<TraceOutput>(begin_itr, end_itr);
  }
}

void CodeTraceDialog::DisplayTrace()
{
  m_output_list->clear();
  u32 results_limit = m_results_limit_input->value();

  // Setup start and end for a changed range, 0 means use full range.
  u32 start = 0;
  u32 end = 0;

  if (m_change_range->isChecked())
  {
    bool good;
    start = m_bp1->currentText().toUInt(&good, 16);
    if (!good)
      start = m_bp1->currentData().toUInt(&good);
    if (!good)
    {
      start = 0;
      new QListWidgetItem(tr("Input error with starting address."), m_output_list);
    }

    end = m_bp2->currentText().toUInt(&good, 16);
    if (!good)
      end = m_bp2->currentData().toUInt(&good);
    if (!good)
    {
      end = 0;
      new QListWidgetItem(tr("Input error with ending address."), m_output_list);
    }
  }

  // Setup memory or register to track
  std::optional<std::string> track_reg = std::nullopt;
  std::optional<u32> track_mem = std::nullopt;

  if (m_trace_target->text().size() == 8)
  {
    bool ok;
    track_mem = m_trace_target->text().toUInt(&ok, 16);

    if (!ok)
    {
      new QListWidgetItem(tr("Memory Address input error"), m_output_list);
      return;
    }
  }
  else if (m_trace_target->text().size() < 5)
  {
    QString reg_tmp = m_trace_target->text();
    reg_tmp.replace(QStringLiteral("sp"), QStringLiteral("r1"), Qt::CaseInsensitive);
    reg_tmp.replace(QStringLiteral("rtoc"), QStringLiteral("r2"), Qt::CaseInsensitive);
    track_reg = reg_tmp.toStdString();
  }
  else
  {
    new QListWidgetItem(tr("Register input error"), m_output_list);
  }

  // Either use CodePath to display the full trace (limited by results_limit) or track a value
  // through the full trace using Forward/Back Trace.
  std::vector<TraceOutput> trace_out;

  if (m_trace_target->text().isEmpty())
  {
    trace_out = CodePath(start, end, results_limit);
  }
  else if (m_backtrace->isChecked())
  {
    trace_out = CT.Backtrace(&m_code_trace, track_reg, track_mem, start, end, results_limit,
                             m_verbose->isChecked());
  }
  else
  {
    trace_out = CT.ForwardTrace(&m_code_trace, track_reg, track_mem, start, end, results_limit,
                                m_verbose->isChecked());
  }

  // Errors to display
  if (!m_error_msg.isEmpty())
    new QListWidgetItem(m_error_msg, m_output_list);

  if (m_code_trace.size() >= m_record_limit)
    new QListWidgetItem(tr("Trace max limit reached, backtrace won't work."), m_output_list);

  if (trace_out.size() >= results_limit)
    new QListWidgetItem(tr("Max output size reached, stopped early"), m_output_list);

  // Update UI
  m_record_limit_label->setText(
      QStringLiteral("Recorded: %1 of").arg(QString::number(m_code_trace.size())));
  m_results_limit_label->setText(
      QStringLiteral("Results: %2 of").arg(QString::number(trace_out.size())));

  // Cleanup and prepare output, then send to Qlistwidget.
  std::regex reg("(\\S*)\\s+(?:(\\S{0,6})\\s*)?(?:(\\S{0,8})\\s*)?(?:(\\S{0,8})\\s*)?(.*)");
  std::smatch match;
  std::string is_mem;

  for (auto out : trace_out)
  {
    QString fix_sym = QString::fromStdString(g_symbolDB.GetDescription(out.address));
    fix_sym.replace(QStringLiteral("\t"), QStringLiteral("  "));

    std::regex_search(out.instruction, match, reg);
    std::string match4 = match.str(4);

    if (out.memory_target)
    {
      is_mem = fmt::format("{:08x}", out.memory_target);

      // There's an extra comma for psq read/writes.
      if (match4.find(',') != std::string::npos)
        match4.pop_back();
    }
    else
    {
      is_mem = match.str(5);
    }

    auto* item =
        new QListWidgetItem(QString::fromStdString(fmt::format(
                                "{:08x} : {:<11}{:<6}{:<8}{:<8}{:<18}", out.address, match.str(1),
                                match.str(2), match.str(3), match4, is_mem)) +
                            fix_sym);

    item->setData(ADDRESS_ROLE, out.address);
    if (out.memory_target)
      item->setData(MEM_ADDRESS_ROLE, out.memory_target);
    m_output_list->addItem(item);
  }
}

void CodeTraceDialog::OnChangeRange()
{
  if (!m_change_range->isChecked())
  {
    m_bp1->setCurrentIndex(0);
    m_bp2->setCurrentIndex(0);
    m_bp1->setEnabled(false);
    m_bp2->setEnabled(false);
    return;
  }

  u32 bp1 = m_bp1->currentData().toUInt();
  u32 bp2 = m_bp2->currentData().toUInt();

  m_bp1->setEnabled(true);
  m_bp2->setEnabled(true);

  m_bp1->setEditText(QStringLiteral("%1").arg(bp1, 8, 16, QLatin1Char('0')));
  m_bp2->setEditText(QStringLiteral("%1").arg(bp2, 8, 16, QLatin1Char('0')));
}

void CodeTraceDialog::UpdateBreakpoints()
{
  // Leave the recorded start and end range intact.
  if (m_record_trace->isChecked())
  {
    for (int i = m_bp2->count(); i > 1; i--)
      m_bp2->removeItem(1);
    for (int i = m_bp1->count(); i > 1; i--)
      m_bp1->removeItem(1);
  }
  else
  {
    m_bp2->clear();
  }

  auto bp_vec = PowerPC::breakpoints.GetBreakPoints();
  int index = -1;

  for (auto& i : bp_vec)
  {
    QString instr = QString::fromStdString(PowerPC::debug_interface.Disassemble(i.address));
    instr.replace(QStringLiteral("\t"), QStringLiteral(" "));
    if (m_record_trace->isChecked())
    {
      m_bp1->addItem(QStringLiteral("%1 : %2").arg(i.address, 8, 16, QLatin1Char('0')).arg(instr),
                     i.address);
    }
    m_bp2->addItem(QStringLiteral("%1 : %2").arg(i.address, 8, 16, QLatin1Char('0')).arg(instr),
                   i.address);
    index++;
  }

  // User typically wants the most recently placed breakpoint.
  if (!m_record_trace->isChecked())
    m_bp2->setCurrentIndex(index);
}

void CodeTraceDialog::InfoDisp()
{
  // i18n: Here, PC is an acronym for program counter, not personal computer.
  new QListWidgetItem(
      QStringLiteral(
          "Used to track a target register or memory address and its uses.\n\nRecord Trace: "
          "Records "
          "each executed instruction while stepping from "
          "PC to selected Breakpoint.\n    Required before tracking a target. If backtracing, set "
          "PC "
          "to how far back you want to trace to.\n    and breakpoint the instruction you want to "
          "trace backwards.\n\nRegister: Input "
          "examples: "
          "r5, f31, use f for ps registers or 80000000 for memory.\n    Only takes one value at a "
          "time. Leave blank "
          "to "
          "view complete "
          "code path. \n\nStarting Address: "
          "Used to change range before tracking a value.\n    Record Trace's starting address "
          "is always "
          "the "
          "PC."
          " Can change freely after recording trace.\n\nEnding breakpoint: "
          "Where "
          "the trace will stop. If backtracing, should be the line you want to backtrace "
          "from.\n\nBacktrace: A reverse trace that shows where a value came from, the first "
          "output "
          "line "
          "is the most recent executed.\n\nVerbose: Will record all references to what is being "
          "tracked, rather than just where it is moving to or from.\n\nReset on loopback: Will "
          "clear "
          "the "
          "trace "
          "if starting address is looped through,\n    ensuring only the final loop to the end "
          "breakpoint is recorded.\n\nChange Range: Change the start and end points of the trace "
          "for tracking. Loops may make certain ranges buggy.\n\nTrack target: Follows the "
          "register or memory value through the recorded trace.\n    You don't "
          "have "
          "to "
          "record a trace multiple times if "
          "the "
          "first trace recorded the area of code you need.\n    You can change any value or option "
          "and "
          "press track target again.\n    Changing the second "
          "breakpoint"
          "will let you backtrace from a new location."),
      m_output_list);
}

void CodeTraceDialog::OnContextMenu()
{
  QMenu* menu = new QMenu(this);
  menu->addAction(tr("Copy &address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->addAction(tr("Copy &memory address"), this, [this]() {
    const u32 addr = m_output_list->currentItem()->data(MEM_ADDRESS_ROLE).toUInt();
    QApplication::clipboard()->setText(QStringLiteral("%1").arg(addr, 8, 16, QLatin1Char('0')));
  });
  menu->exec(QCursor::pos());
}
