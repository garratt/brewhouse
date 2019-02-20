// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>
#include <deque>
#include <vector>
#include <mutex>


struct BrewRecipe {
  std::string session_name;
  std::vector<double> mash_temps;
  std::vector<uint32_t> mash_times;
  unsigned boil_minutes = 0;
  double grain_weight_grams;
  double initial_volume_liters = 0, sparge_liters = 0;

  void Print();

  // This creates the string which is passed to the Grainfather to
  // Load a session
  std::string GetSessionCommand();
};

namespace oauth {
class OathAccess;
} // namespace oauth

// This class logs data to a google spreadsheet
class BrewLogger {
 public:

  BrewLogger(const char *spreadsheet_id = "");
  ~BrewLogger();

  const char *levels_[5] = {"Debug", "Info", "Warning", "Error", "Fatal"};
  void Log(int severity, std::string message);

  void LogWeight(double grams, time_t log_time = 0);

  BrewRecipe ReadRecipe();

 private:
  static constexpr const char *kSheetsScope = "https://www.googleapis.com/auth/spreadsheets";
  static constexpr const char *kDriveScope = "https://www.googleapis.com/auth/drive";

  // ---------------------------------------------
  //   Brew Session Layout
  // This pulls brew session info from specific places

  static constexpr const char *kLayoutVersionLoc = "Overview!K1";
  static constexpr const int kLayoutVersion = 1;
  static constexpr const char *kSessionNameLoc = "Overview!A2";
  static constexpr const char *kNumMashStepsLoc = "Overview!G3";
  static constexpr const char *kMashTimesLoc = "Overview!H5:H9";
  static constexpr const char *kMashTempsLoc = "Overview!G5:G9";
  static constexpr const char *kBoilTimeLoc = "Overview!G11";
  static constexpr const char *kGrainWeightLoc = "Overview!B7";
  static constexpr const char *kInitialVolumeLoc = "Overview!G14";
  static constexpr const char *kSpargeVolumeLoc = "Overview!G15";
  static constexpr const char *kWaterVolumesLoc = "Overview!G14:G15";
  static constexpr const char *kLogRange = "Log!A2:E3";
  static constexpr const char *kWeightRange = "weights!A2:E3";

  std::string spreadsheet_id_;
  std::unique_ptr<oauth::OathAccess> sheets_access_, drive_access_;
  // For threaded operation:
  struct LogMessage {
   std::string cell_range, sheet_id, values;
  };
  std::deque<LogMessage> message_queue_;
  bool quit_threads_ = false;
  std::mutex message_lock_;
  std::thread message_thread_;

  bool PopMessage(LogMessage *current_message);

  void SendMessages();

  void EnqueueMessage(std::string cell_range, std::string sheet_id, std::string values);

  std::vector<std::string> GetValues(std::string range);
  // just one value
  std::string GetValue(std::string range);
};
