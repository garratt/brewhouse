// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logger.h"

#include <iostream>
#include <stdio.h>
#include <sys/types.h>  // for open
#include <sys/stat.h>   // for open
#include <unistd.h>     // for read
#include <fcntl.h>      // for open
#include <curl/curl.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <thread>
#include <deque>
#include <mutex>
#include <memory>

namespace oauth {

//  libcurl write callback function
static int writer(char *data, size_t size, size_t nmemb,
                  std::string *writerData) {
  if (writerData == NULL) return 0;
  writerData->append(data, size * nmemb);
  return size * nmemb;
}

// Basic posting data:
std::string CurlPost(const char *url, const char *post_data,
                     curl_slist *headers = NULL) {
  CURLcode res;
  curl_global_init(CURL_GLOBAL_ALL);

  /* get a curl handle */
  CURL *curl = curl_easy_init();
  if (!curl) {
    printf("failed to initialize curl handle\n");
    curl_global_cleanup();
    return "";
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  if (post_data) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
  }
  static char errorBuffer[CURL_ERROR_SIZE];
  res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
  if (res != CURLE_OK) {
    fprintf(stderr, "Failed to set error buffer [%d]\n", res);
    return "";
  }
  res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
  if (res != CURLE_OK) {
    fprintf(stderr, "Failed to set writer [%s]\n", errorBuffer);
    return "";
  }
  std::string buffer;
  res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  if (res != CURLE_OK) {
    fprintf(stderr, "Failed to set write data [%s]\n", errorBuffer);
    return "";
  }
  /* Perform the request, res will get the return code */
  res = curl_easy_perform(curl);
  /* Check for errors */
  if (res != CURLE_OK)
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));

  /* always cleanup */
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return buffer;
}

void AppendToSheets(const char *range, const char *sheet, const char *values,
                    const char *token) {
  char url[2000], token_header[500];
  const char *url_format =
      "https://sheets.googleapis.com/v4/spreadsheets/%s/values/"
      "%s:append?valueInputOption=USER_ENTERED";
  snprintf(url, 2000, url_format, sheet, range);
  snprintf(token_header, 500, "Authorization: Bearer %s", token);
  curl_slist *list = NULL;
  list = curl_slist_append(list, "Content-Type: application/json");
  list = curl_slist_append(list, token_header);
  std::string response = CurlPost(url, values, list);
  if (response.size() == 0) {
    printf("Failed to put data in spreadsheet\n");
    return;
  }
  // TODO: check for other errors?
}

std::string ReadFromSheets(const char *range, const char *sheet,
                           const char *token) {
  char url[2000], token_header[500];
  const char *url_format =
      "https://sheets.googleapis.com/v4/spreadsheets/%s/values/%s";
  // ?majorDimension=COLUMNS";
  snprintf(url, 2000, url_format, sheet, range);
  snprintf(token_header, 500, "Authorization: Bearer %s", token);
  curl_slist *list = NULL;
  list = curl_slist_append(list, "Content-Type: application/json");
  list = curl_slist_append(list, token_header);
  std::string response = CurlPost(url, nullptr, list);
  if (response.size() == 0) {
    printf("Failed to read data from spreadsheet\n");
    return "";
  }
  // TODO: check for other errors?
  // Return json string
  return response;
}

// Read One value
std::string ReadValueFromSheets(const char *range, const char *sheet,
                                const char *token) {
  std::string response = ReadFromSheets(range, sheet, token);
  rapidjson::Document document;
  document.Parse(response.c_str());
  if (!document.IsObject()) {
    printf("Document is not object\n");
    return "";
  }
  if (document.HasMember("values") && document["values"].IsArray()) {
    return document["values"][0][0].GetString();
  } else {
    printf("failed to get values\n");
  }
  return "";
}

// Read One value
std::vector<std::string> ReadArrayFromSheets(const char *range,
                                             const char *sheet,
                                             const char *token) {
  std::string response = ReadFromSheets(range, sheet, token);
  std::vector<std::string> ret;
  rapidjson::Document document;
  document.Parse(response.c_str());
  if (!document.IsObject()) {
    printf("Document is not object\n");
    return ret;
  }
  if (document.HasMember("values") && document["values"].IsArray()) {
    for (unsigned i = 0; i < document["values"].Size(); ++i) {
      ret.push_back(document["values"][i][0].GetString());
    }
  }
  return ret;
}

// Returns the id of the new sheet
std::string CopySheet(const char *title, const char *sheet, const char *token) {
  char url[2000], token_header[500];
  const char *url_format = "https://www.googleapis.com/drive/v2/files/%s/copy";
  snprintf(url, 2000, url_format, sheet);
  snprintf(token_header, 500, "Authorization: Bearer %s", token);
  curl_slist *list = NULL;
  list = curl_slist_append(list, "Content-Type: application/json");
  list = curl_slist_append(list, token_header);
  char values[500];
  sprintf(values, "{\"title\": \"%s\"}", title);

  std::string response = CurlPost(url, values, list);
  printf("%s", response.c_str());
  if (response.size() == 0) {
    printf("Failed to put data in spreadsheet\n");
    return "";
  }
  // Parse the response to get the new sheet id
  rapidjson::Document document;
  document.Parse(response.c_str());
  if (!document.IsObject()) {
    printf("Document is not object\n");
    printf("%s", response.c_str());
    return "";
  }
  if (document.HasMember("id") && document["id"].IsString()) {
    return document["id"].GetString();
  }
  return "";
}

struct Tokens {
  std::string access_token, refresh_token;
  time_t last_refresh;  // not portable, but represents seconds
                        // from some fixed time, which is enough
  Tokens() { last_refresh = 0; }

  void SetAccessToken(const std::string &new_access_token) {
    if (new_access_token.size() == 0) {
      printf("Bad access token.");
      last_refresh = 0;
    }
    access_token = new_access_token;
    last_refresh = time(NULL);
  }

  bool isExpired() { return (time(NULL) - last_refresh) > 3600; }

  bool HasValidAccess() { return !isExpired() && access_token.size() > 0; }

  bool HasValidRefresh() { return refresh_token.size() > 0; }

  void Save(const char *filename) {
    FILE *fp = fopen(filename, "w+");
    fprintf(fp, "%s\n%s\n%ld\n", access_token.c_str(), refresh_token.c_str(),
            last_refresh);
    fclose(fp);
  }

  void Load(const char *filename) {
    char atoken[500], rtoken[500];
    FILE *fp = fopen(filename, "rb");
    if (!fp) return;
    fscanf(fp, "%s\n%s\n%ld", atoken, rtoken, &last_refresh);
    fclose(fp);
    access_token = atoken;
    refresh_token = rtoken;
  }
};

struct Credentials {
  std::string client_id, client_secret;
  std::string token_uri, redirect_uri, auth_uri;
  static constexpr const char *kDefaultFilename = "credentials.json";
  bool IsValid() {
    return (client_id.size() > 0) && (client_secret.size() > 0);
  }
  void Load(const char *filename = kDefaultFilename) {
    char cred_buff[1024];
    int fd = open(filename, O_RDONLY);
    if (fd <= 0) {
      printf("failed to open credentials.json\n");
      return;
    }
    int bytes = read(fd, cred_buff, 1024);
    cred_buff[bytes] = '\0';
    close(fd);
    rapidjson::Document document;
    document.Parse(cred_buff);
    if (!document.IsObject()) {
      printf("Document is not object\n");
      return;
    }
    if (document.HasMember("installed") &&
        document["installed"].HasMember("client_id")) {
      client_id = document["installed"]["client_id"].GetString();
      client_secret = document["installed"]["client_secret"].GetString();
      token_uri = document["installed"]["token_uri"].GetString();
      auth_uri = document["installed"]["auth_uri"].GetString();
      redirect_uri = document["installed"]["redirect_uris"][0].GetString();
    } else {
      printf("failed to get credentials\n");
    }
  }
};

std::string ParseRefreshToken(std::string resp) {
  rapidjson::Document document;
  document.Parse(resp.c_str());
  if (!document.IsObject()) {
    printf("Document is not object\n");
    printf("%s", resp.c_str());
    return "";
  }
  if (document.HasMember("refresh_token") &&
      document["refresh_token"].IsString()) {
    return document["refresh_token"].GetString();
  }
  return "";
}

std::string ParseAccessToken(std::string resp) {
  rapidjson::Document document;
  document.Parse(resp.c_str());
  if (!document.IsObject()) {
    printf("Document is not object\n");
    printf("%s", resp.c_str());
    return "";
  }
  if (document.HasMember("access_token") &&
      document["access_token"].IsString()) {
    return document["access_token"].GetString();
  }
  return "";
}

class OathAccess {
  Tokens tokens_;
  Credentials creds_;
  const char *scope_;
  const char *prefix_;
  std::string tokens_path_;

 public:
  OathAccess(const char *scope, const char *prefix)
      : scope_(scope), prefix_(prefix) {
    creds_.Load();
    if (!creds_.IsValid()) {
      printf("Could not load credentials!\n");
    }
    tokens_path_ = prefix;
    tokens_path_ += "_tokens.txt";
    tokens_.Load(tokens_path_.c_str());
  }

  void RefreshAuthToken() {
    char post_data[2000];
    const char *data_format =
        "client_id=%s&client_secret=%s&refresh_token=%s"
        "&redirect_uri=%s&grant_type=refresh_token";
    snprintf(post_data, 2000, data_format, creds_.client_id.c_str(),
             creds_.client_secret.c_str(), tokens_.refresh_token.c_str(),
             creds_.redirect_uri.c_str());
    // const char *url = "https://accounts.google.com/o/oauth2/token";
    tokens_.SetAccessToken(
        ParseAccessToken(CurlPost(creds_.token_uri.c_str(), post_data)));
  }

  // This will get us auth and refresh token
  std::string GetAccessToken() {
    // First off, if our token is still valid, just return it.
    if (tokens_.HasValidAccess()) return tokens_.access_token;
    // const char *scope = "https://www.googleapis.com/auth/spreadsheets";
    // First, check if we have a refresh code:
    if (tokens_.HasValidRefresh()) {
      // try just refreshing token.  This should work unless the user has
      // revoked it.
      RefreshAuthToken();
      if (tokens_.HasValidAccess()) return tokens_.access_token;
    }
    // Otherwise, we have to get a new client code. This will take user input.
    const char *url_format =
        "%s?client_id=%s&redirect_uri=%s&scope=%s&response_type=code";
    printf("Get the code for authentification.  Go to:\n");
    printf(url_format, creds_.auth_uri.c_str(), creds_.client_id.c_str(),
           creds_.redirect_uri.c_str(), scope_);
    printf("\n\nPaste the code here:\n");
    char code[200];
    scanf("%s", code);

    // Now use the code to get a new access and refresh code:
    char post_data[2000];
    const char *data_format =
        "code=%s&client_id=%s&client_secret=%s"
        "&redirect_uri=%s&grant_type=authorization_code";
    snprintf(post_data, 2000, data_format, code, creds_.client_id.c_str(),
             creds_.client_secret.c_str(), creds_.redirect_uri.c_str());
    // const char *url = "https://accounts.google.com/o/oauth2/token";
    std::string response = CurlPost(creds_.token_uri.c_str(), post_data);
    tokens_.refresh_token = ParseRefreshToken(response);
    tokens_.SetAccessToken(ParseAccessToken(response));
    if (!tokens_.HasValidAccess()) {
      printf("Getting Acces token failed!\n");
    }
    // We just got a new refresh token, so save that to file
    if (tokens_.HasValidRefresh()) tokens_.Save(tokens_path_.c_str());
    return tokens_.access_token;
  }
};

}  // namespace oauth


bool BrewLogger::PopMessage(LogMessage *current_message) {
  std::lock_guard<std::mutex> lock(message_lock_);
  if (message_queue_.size() == 0) {
    return false;
  }
  *current_message = message_queue_.front();
  message_queue_.pop_front();
  return true;
}

void BrewLogger::SendMessages() {
  LogMessage current_message;
  while (!quit_threads_) {
    if (PopMessage(&current_message)) {
      oauth::AppendToSheets(current_message.cell_range.c_str(),
                            current_message.sheet_id.c_str(),
                            current_message.values.c_str(),
                            sheets_access_->GetAccessToken().c_str());
    } else {
      sleep(1);
    }
  }
}

void BrewLogger::EnqueueMessage(std::string cell_range, std::string sheet_id,
                                std::string values) {
  std::lock_guard<std::mutex> lock(message_lock_);
  LogMessage message = {cell_range, sheet_id, values};
  message_queue_.push_back(message);
}

BrewLogger::BrewLogger() {
  for (int i = 0; i < StageEvent::NumStates; ++i) {
    logged_stage_[i] = false;
  }
}

// TODO: do checks to make sure the sheet is valid
int BrewLogger::SetSession(const char *spreadsheet_id) {
  if (disable_for_test_) return 0;
  const char *kSheetsScope = "https://www.googleapis.com/auth/spreadsheets";
  const char *kDriveScope = "https://www.googleapis.com/auth/drive";
  sheets_access_ = std::make_unique<oauth::OathAccess>(kSheetsScope, "sheets");
  drive_access_ = std::make_unique<oauth::OathAccess>(kDriveScope, "drive");

  const char *template_sheet = "1XKuW8LUqdtQWHElJse4nhc9-g7gZZex1t9i_oJFn5FQ";
  // Copy the template into a new sheet
  // spreadsheet_id_ = oauth::CopySheet(session_name, template_sheet,
  // drive_access_.GetAccessToken().c_str());
  if (strlen(spreadsheet_id) > 10) {
    spreadsheet_id_ = spreadsheet_id;
  } else {
    spreadsheet_id_ = template_sheet;
  }
  // because std::thread is movable, just assign another one there:
  message_thread_ = std::thread(&BrewLogger::SendMessages, this);
  return 0;
}


BrewLogger::~BrewLogger() {
  quit_threads_ = true;
  if (disable_for_test_) return;
  if (message_thread_.joinable()) {
    message_thread_.join();
  }
}

void BrewLogger::Log(int severity, std::string message) {
  if (disable_for_test_) return;
  // take timestamp
  timespec tm;
  clock_gettime(CLOCK_REALTIME, &tm);
  char values[2000];
  // time (readable), time(number), severity, message
  const char *values_format =
      "{\"values\":[[\"%s\", \"%d.%09ld\", \"%s\", \"%s\"]]}";
  sprintf(values, values_format, ctime(&tm.tv_sec), tm.tv_sec, tm.tv_nsec,
          levels_[severity], message.c_str());
  EnqueueMessage(kLogRange, spreadsheet_id_.c_str(), values);
  // TODO: also log to file
}


void BrewLogger::LogWeightEvent(WeightEvent event_id, double grams) {
  // TODO: check for invalid values?
  char values[30];
  char range[15];
  snprintf(values, 30, "{\"values\":[[\"%lf\"]]}", grams);
  snprintf(range, 15, kWeightEventFormat, kWeightEventStartRow + (int)event_id);
  EnqueueMessage(range, spreadsheet_id_.c_str(), values);
}

void BrewLogger::LogStageEvent(StageEvent event_id) {
  if (event_id >= StageEvent::NumStates || logged_stage_[event_id]) {
    return;
  }
  // TODO: check for invalid values?
  char values[30];
  char range[15];
  time_t t = time(NULL);
  struct tm *tmp = localtime(&t);
  strftime(values, 30, "{\"values\":[[\"%T\"]]}", tmp);
  snprintf(range, 15, kStageEventFormat, kStageEventStartRow + (int)event_id);
  EnqueueMessage(range, spreadsheet_id_.c_str(), values);

  // If we just loaded the session, lets call the brew day today.
  if (event_id == StageEvent::LoadedSession) {
    strftime(values, 30, "{\"values\":[[\"%T\"]]}", tmp);
    EnqueueMessage(kBrewDateLoc, spreadsheet_id_.c_str(), values);
  }

}


void BrewLogger::LogWeight(double grams, time_t log_time) {
  if (disable_for_test_) return;
  // time, time, weight
  timespec tm;
  if (log_time == 0) {
    clock_gettime(CLOCK_REALTIME, &tm);
  } else {
    tm.tv_sec = log_time;
    tm.tv_nsec = 0;
  }
  char values[2000];
  // time (readable), time(number), severity, message
  const char *values_format = "{\"values\":[[\"%s\", \"%d.%09ld\", \"%f\"]]}";
  sprintf(values, values_format, ctime(&tm.tv_sec), tm.tv_sec, tm.tv_nsec,
          grams);
  EnqueueMessage(kWeightRange, spreadsheet_id_.c_str(), values);
}

std::vector<std::string> BrewLogger::GetValues(std::string range) {
  return oauth::ReadArrayFromSheets(range.c_str(), spreadsheet_id_.c_str(),
                                    sheets_access_->GetAccessToken().c_str());
}
// just one value
std::string BrewLogger::GetValue(std::string range) {
  return oauth::ReadValueFromSheets(range.c_str(), spreadsheet_id_.c_str(),
                                    sheets_access_->GetAccessToken().c_str());
}


BrewRecipe fake_recipe = {
 .session_name = "Fake Recipe",
 .mash_temps = {45.0, 60.0},
 .mash_times = {1, 2},
 .boil_minutes = 20,
 .grain_weight_grams = 8000,
 .hops_grams = 56,
 .hops_type = "Bestest Hops",
 .initial_volume_liters = 23.0,
 .sparge_liters = 0.5 };



// TODO: Add hops amount and type
BrewRecipe BrewLogger::ReadRecipe() {
  if (disable_for_test_) return fake_recipe;
  BrewRecipe recipe;
  recipe.session_name = GetValue(kSessionNameLoc);
  recipe.boil_minutes = atoi(GetValue(kBoilTimeLoc).c_str());
  recipe.grain_weight_grams = atof(GetValue(kGrainWeightLoc).c_str()) * 1000.0;
  recipe.hops_grams = atof(GetValue(kHopsWeightLoc).c_str());
  recipe.hops_type = GetValue(kHopsTypeLoc);
  std::vector<std::string> mash_times, mash_temps, volumes;
  mash_temps = GetValues(kMashTempsLoc);
  mash_times = GetValues(kMashTimesLoc);
  volumes = GetValues(kWaterVolumesLoc);
  recipe.initial_volume_liters = atof(volumes[0].c_str());
  recipe.sparge_liters = atof(volumes[1].c_str());
  unsigned mash_steps = mash_temps.size();
  if (mash_temps.size() != mash_times.size()) {
    printf("Size of mash temps != mash times\n");
    if (mash_temps.size() > mash_times.size()) {
      mash_steps = mash_times.size();
    }
  }
  for (unsigned i = 0; i < mash_steps; ++i) {
    recipe.mash_temps.push_back(atof(mash_temps[i].c_str()));
    recipe.mash_times.push_back(atoi(mash_times[i].c_str()));
  }
  return recipe;
}

std::string ToValue(bool v) {
  if (v) {
    return "\"1\", ";
  }
  return "\"0\", ";
}
std::string ToValue(double d) {
  char ret[20];
  snprintf(ret, 20, "\"%4.5lf\", ", d);
  return ret;
}
std::string ToValue(uint32_t d) {
  char ret[20];
  snprintf(ret, 20, "\"%u\", ", d);
  return ret;
}



void BrewLogger::LogBrewState(const BrewState &state) {
  if (disable_for_test_) return;
  // time, time, weight
  // Log Stage events based on the Brewstate:
  // The input_reason is seldom non-zero, so we just
  // try to send the info every time.  The StageEvent
  // is only logged during the first call.
  switch (state.input_reason) {
    case BrewState::InputReason::StartHeating:
      LogStageEvent(StageEvent::LoadedSession);
      break;
    case BrewState::InputReason::StartMash:
      LogStageEvent(StageEvent::MashAtTemp);
      break;
    case BrewState::InputReason::StartSparge:
      LogStageEvent(StageEvent::MashDone);
      break;
    case BrewState::InputReason::StartBoil:
      LogStageEvent(StageEvent::BoilStart);
      break;
    case BrewState::InputReason::FinishSession:
      LogStageEvent(StageEvent::BoilDone);
      break;
  }

  timespec tm;
  clock_gettime(CLOCK_REALTIME, &tm);
  char values[200];
  // log each field as a column
  // read/relative time | global time
  // timer on | timer paused | total sec | sec left
  // wait input | brew session loaded | stage | input_reason
  // wait temp | target temp | current temp |heat on | %heat
  // pump on
  const char *values_format = "{\"values\":[[\"%s\", \"%ld\", ";
  sprintf(values, values_format, ctime(&tm.tv_sec), state.read_time);
  std::string sval(values);
  sval += ToValue(state.brew_session_loaded);
  sval += ToValue(state.stage);
  sval += ToValue(state.input_reason);
  sval += ToValue(state.timer_on);
  sval += ToValue(state.timer_paused);
  sval += ToValue(state.timer_total_seconds);
  sval += ToValue(state.timer_seconds_left);
  sval += ToValue(state.waiting_for_input);
  sval += ToValue(state.waiting_for_temp);
  sval += ToValue(state.heater_on);
  sval += ToValue(state.current_temp);
  sval += ToValue(state.target_temp);
  sval += ToValue(state.percent_heating);
  sval += ToValue(state.pump_on);
  sval += " \"1\"]] }";  // Version
  EnqueueMessage(kBrewStateRange, spreadsheet_id_.c_str(), sval.c_str());
}
