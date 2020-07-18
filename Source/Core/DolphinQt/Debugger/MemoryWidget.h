// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QDockWidget>

#include "Common/CommonTypes.h"

class MemoryViewWidget;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QShowEvent;
class QSplitter;

class MemoryWidget : public QDockWidget
{
  Q_OBJECT
public:
  explicit MemoryWidget(QWidget* parent = nullptr);
  ~MemoryWidget();

  void SetAddress(u32 address);
  void Update();
signals:
  void BreakpointsChanged();
  void ShowCode(u32 address);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void OnTypeChanged();
  void OnBPLogChanged();
  void OnBPTypeChanged();

  void OnAlignmentChanged();

  void OnSearchAddress();
  void OnFindNextValue();
  void OnFindPreviousValue();
  void ValidateSearchValue();

  void OnSetValue();

  void OnSearchNotes();
  void OnSelectNote();
  void UpdateNotes();

  void OnDumpMRAM();
  void OnDumpExRAM();
  void OnDumpARAM();
  void OnDumpFakeVMEM();

  void OnFloatToHex(bool float_in);

  std::vector<u8> GetValueData() const;

  void FindValue(bool next);

  void closeEvent(QCloseEvent*) override;
  void showEvent(QShowEvent* event) override;

  MemoryViewWidget* m_memory_view;
  QSplitter* m_splitter;
  QLineEdit* m_search_address;
  QLineEdit* m_search_address_offset;
  QLineEdit* m_data_edit;
  QLabel* m_data_preview;
  QPushButton* m_set_value;
  QPushButton* m_dump_mram;
  QPushButton* m_dump_exram;
  QPushButton* m_dump_aram;
  QPushButton* m_dump_fake_vmem;

  // Search
  QCheckBox* m_ignore_case;
  QPushButton* m_find_next;
  QPushButton* m_find_previous;
  QRadioButton* m_input_ascii;
  QRadioButton* m_input_float;
  QRadioButton* m_input_hex;
  QLabel* m_result_label;
  QCheckBox* m_find_mem2;

  // Align table
  QCheckBox* m_align_switch;

  // Datatypes
  QRadioButton* m_type_u8;
  QRadioButton* m_type_u16;
  QRadioButton* m_type_u32;
  QRadioButton* m_type_ascii;
  QRadioButton* m_type_float;
  QCheckBox* m_mem_view_style;

  // Breakpoint options
  QRadioButton* m_bp_read_write;
  QRadioButton* m_bp_read_only;
  QRadioButton* m_bp_write_only;
  QCheckBox* m_bp_log_check;

  QGroupBox* m_note_group;
  QLineEdit* m_search_notes;
  QListWidget* m_note_list;
  QString m_note_filter;

  // Float to Hex conversion
  QLineEdit* m_float_convert;
  QLineEdit* m_hex_convert;
};
