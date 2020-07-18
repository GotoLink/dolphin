// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"

class ARCodeWidget;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSplitter;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;
class QTimer;
struct Result;

struct Ram
{
  const u8* ptr = nullptr;
  u32 size;
  u32 base;
};

namespace Core
{
enum class State;
}

namespace UICommon
{
class GameFile;
}

class CheatsManager : public QDialog
{
  Q_OBJECT

public:
  explicit CheatsManager(QWidget* parent = nullptr);
  ~CheatsManager();
  void reject() override;

private:
  class Result final
  {
  public:
    Result() : address(0), old_value(0) {}
    u32 address;
    u32 old_value;
  };

  QWidget* CreateCheatSearch();
  void MemoryPtr(bool update = true);
  void CreateWidgets();
  void ConnectWidgets();
  void OnStateChanged(Core::State state);

  int GetTypeSize() const;
  void OnMatchContextMenu();
  void Reset();
  void FilterCheatSearchResults(u32 value, bool prev);
  void OnNewSearchClicked();
  void NextSearch();
  u32 SwapValue(u32 value);
  void TimedUpdate();
  void Update();

  Ram m_ram;
  std::vector<Result> m_results;
  std::shared_ptr<const UICommon::GameFile> m_game_file;
  QDialogButtonBox* m_button_box;
  QTabWidget* m_tab_widget = nullptr;

  QWidget* m_cheat_search;
  ARCodeWidget* m_ar_code = nullptr;

  QLabel* m_result_label;
  QTableWidget* m_match_table;
  QSplitter* m_option_splitter;
  QSplitter* m_table_splitter;
  QComboBox* m_match_length;
  QComboBox* m_match_operation;
  QLineEdit* m_match_value;
  QLineEdit* m_range_start;
  QLineEdit* m_range_end;
  QLineEdit* m_refresh;
  QLabel* m_refresh_label;
  QPushButton* m_match_new;
  QPushButton* m_match_next;
  QPushButton* m_match_refresh;
  QPushButton* m_match_reset;
  QTimer* m_timer;

  QRadioButton* m_match_decimal;
  QRadioButton* m_match_hexadecimal;
  QRadioButton* m_match_octal;
  QRadioButton* m_ram_main;
  QRadioButton* m_ram_wii;
  QRadioButton* m_ram_fakevmem;
  bool m_updating = false;
  int m_search_type_size;
  bool m_scan_is_initialized = false;
};
