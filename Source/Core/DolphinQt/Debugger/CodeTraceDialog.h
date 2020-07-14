// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

#include "Common/Debug/CodeTrace.h"

class CodeWidget;
class QCheckBox;
class QLineEdit;
class QLabel;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QSpinBox;

class CodeTraceDialog : public QDialog
{
  Q_OBJECT
public:
  explicit CodeTraceDialog(CodeWidget* parent);

  void reject() override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void ClearAll();
  void OnRecordTrace(bool checked);
  std::vector<TraceOutput> CodePath(u32 start, u32 end, u32 results_limit);
  void DisplayTrace();
  void OnChangeRange();
  void UpdateBreakpoints();
  void InfoDisp();

  void OnContextMenu();

  QListWidget* m_output_list;
  QLineEdit* m_trace_target;
  QComboBox* m_bp1;
  QComboBox* m_bp2;
  QCheckBox* m_backtrace;
  QCheckBox* m_verbose;
  QCheckBox* m_clear_on_loop;
  QCheckBox* m_change_range;
  QPushButton* m_reprocess;
  QLabel* m_record_limit_label;
  QLabel* m_results_limit_label;
  QSpinBox* m_record_limit_input;
  QSpinBox* m_results_limit_input;

  QPushButton* m_record_trace;
  CodeWidget* m_parent;

  CodeTrace CT;
  std::vector<TraceOutput> m_code_trace;
  std::vector<TraceOutput> m_trace_out;

  size_t m_record_limit = 150000;
  QString m_error_msg;

  bool m_recording = false;
};
