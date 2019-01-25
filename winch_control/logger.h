// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


namespace oauth {

 
//  libcurl write callback function
static int writer(char *data, size_t size, size_t nmemb, std::string *writerData) {
  if(writerData == NULL)
    return 0;
  writerData->append(data, size*nmemb);
  return size * nmemb;
}


// Basic posting data:
std::string CurlPost(const char *url, const char *post_data, curl_slist *headers = NULL) {
  CURLcode res;
  curl_global_init(CURL_GLOBAL_ALL);

  /* get a curl handle */
  CURL *curl = curl_easy_init();
  if(!curl) {
    printf("failed to initialize curl handle\n");
    curl_global_cleanup();
    return "";
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

  static char errorBuffer[CURL_ERROR_SIZE];
  res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
  if(res != CURLE_OK) {
    fprintf(stderr, "Failed to set error buffer [%d]\n", res);
    return "";
  }
  res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
  if(res != CURLE_OK) {
    fprintf(stderr, "Failed to set writer [%s]\n", errorBuffer);
    return "";
  }
  std::string buffer;
  res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  if(res != CURLE_OK) {
    fprintf(stderr, "Failed to set write data [%s]\n", errorBuffer);
    return "";
  }
  /* Perform the request, res will get the return code */
  res = curl_easy_perform(curl);
  /* Check for errors */
  if(res != CURLE_OK)
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
        curl_easy_strerror(res));

  /* always cleanup */
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return buffer;
}


void AppendToSheets(const char *range, const char *sheet, const char *values, const char *token) {
  char url[2000], token_header[500];
  const char *url_format = "https://sheets.googleapis.com/v4/spreadsheets/%s/values/%s:append?valueInputOption=USER_ENTERED";
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
  // static constexpr const char * kDefaultFilename = "tokens.txt";
  Tokens() {
    last_refresh = 0;
  }

  void SetAccessToken(const std::string &new_access_token) {
    if (new_access_token.size() == 0) {
      printf("Bad access token.");
      last_refresh = 0;
    }
    access_token = new_access_token;
    last_refresh = time(NULL);
  }

  bool isExpired() {
    return (time(NULL) - last_refresh) > 3600;
  }

  bool HasValidAccess() {
    return !isExpired() && access_token.size() > 0;
  }

  bool HasValidRefresh() {
    return refresh_token.size() > 0;
  }

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
  static constexpr const char * kDefaultFilename = "credentials.json";
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
    // printf("read cred:\n%s\n", cred_buff);
    rapidjson::Document document;
    document.Parse(cred_buff);
    if (!document.IsObject()) {
      printf("Document is not object\n");
      return;
    }
    if (document.HasMember("installed") && document["installed"].HasMember("client_id")) {
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
  if (document.HasMember("refresh_token") && document["refresh_token"].IsString()) {
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
  if (document.HasMember("access_token") && document["access_token"].IsString()) {
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

  OathAccess(const char *scope, const char *prefix) : scope_(scope), prefix_(prefix) {
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
    const char *data_format = "client_id=%s&client_secret=%s&refresh_token=%s"
      "&redirect_uri=%s&grant_type=refresh_token";
    snprintf(post_data, 2000, data_format, creds_.client_id.c_str(),
        creds_.client_secret.c_str(), tokens_.refresh_token.c_str(),
        creds_.redirect_uri.c_str());
    // const char *url = "https://accounts.google.com/o/oauth2/token";
    tokens_.SetAccessToken(ParseAccessToken(CurlPost(creds_.token_uri.c_str(), post_data)));
  }

  // This will get us auth and refresh token
  std::string GetAccessToken() {
    // First off, if our token is still valid, just return it.
    if (tokens_.HasValidAccess())
      return tokens_.access_token;
    // const char *scope = "https://www.googleapis.com/auth/spreadsheets";
    // First, check if we have a refresh code:
    if (tokens_.HasValidRefresh()) {
      // try just refreshing token.  This should work unless the user has revoked it.
      RefreshAuthToken();
      if (tokens_.HasValidAccess())
        return tokens_.access_token;
    }
    // Otherwise, we have to get a new client code. This will take user input.
    const char *url_format = "%s?client_id=%s&redirect_uri=%s&scope=%s&response_type=code";
    printf("Get the code for authentification.  Go to:\n");
    printf(url_format, creds_.auth_uri.c_str(), creds_.client_id.c_str(),
           creds_.redirect_uri.c_str(), scope_);
    printf("\n\nPaste the code here:\n");
    char code[200];
    scanf("%s", code);

    // Now use the code to get a new access and refresh code:
    char post_data[2000];
    const char *data_format = "code=%s&client_id=%s&client_secret=%s"
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
    if (tokens_.HasValidRefresh())
      tokens_.Save(tokens_path_.c_str());
    return tokens_.access_token;
  }
};


} // namespace oauth

// This class logs all data from one brewing session
class BrewLogger {
  static constexpr const char *kSheetsScope = "https://www.googleapis.com/auth/spreadsheets";
  static constexpr const char *kDriveScope = "https://www.googleapis.com/auth/drive";
  // TODO: add sheet
  static constexpr const char *kLogRange = "Log!A2:E3";
  static constexpr const char *kWeightRange = "weights!A2:E3";
  std::string spreadsheet_id_;
  oauth::OathAccess sheets_access_, drive_access_;
  // For threaded operation:
  struct LogMessage {
   std::string cell_range, sheet_id, values;
  };
  std::deque<LogMessage> message_queue_;
  bool quit_threads_ = false;
  std::mutex message_lock_;
  bool PopMessage(LogMessage *current_message) {
     std::lock_guard<std::mutex> lock(message_lock_);
     if (message_queue_.size() == 0) {
       return false;
     }
     *current_message = message_queue_.front();
     message_queue_.pop_front();
     return true;
  }


  void SendMessages() {
    LogMessage current_message;
    while(!quit_threads_) {
     if (PopMessage(&current_message)) {
        oauth::AppendToSheets(current_message.cell_range.c_str(),
                              current_message.sheet_id.c_str(),
                              current_message.values.c_str(),
                              sheets_access_.GetAccessToken().c_str());
     } else {
       sleep(1);
     }
    }
  }

  void EnqueueMessage(std::string cell_range, std::string sheet_id, std::string values) {
     std::lock_guard<std::mutex> lock(message_lock_);
     LogMessage message = {cell_range, sheet_id, values};
     message_queue_.push_back(message);
  }


  std::thread message_thread_;
  public:
  BrewLogger(const char *session_name) : sheets_access_(kSheetsScope, "sheets"), drive_access_(kDriveScope, "drive") {
    // TODO: this is just a test sheet
    const char *template_sheet = "1kOpPnSpVyEKsGWud0Jmv5YKM5OOeepWDgQaWmGNwTyU";
    // Copy the template into a new sheet
    spreadsheet_id_ = oauth::CopySheet(session_name, template_sheet,
                                      drive_access_.GetAccessToken().c_str());

    // because std::thread is movable, just assign another one there:
    message_thread_ = std::thread(&BrewLogger::SendMessages, this);
  }

  ~BrewLogger() {
    quit_threads_ = true;
    message_thread_.join();
  }

  // Sheet Layout:
  // Overview - has agregate of stats for brew
  //   time of mash start
  //   time of mash end
  //   time of boil start, end
  //   Volumes: initial, after mash, after boil
  // Weights - timestamp and weight
  // Error/Debug/Info Log

  const char *levels_[5] = {"Debug", "Info", "Warning", "Error", "Fatal"};
  void Log(int severity, std::string message) {
    //take timestamp
    timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);
    char values[2000];
    // time (readable), time(number), severity, message
    const char *values_format = "{\"values\":[[\"%s\", \"%d.%09ld\", \"%s\", \"%s\"]]}";
    sprintf(values, values_format, ctime(&tm.tv_sec), tm.tv_sec, tm.tv_nsec,
        levels_[severity], message.c_str());
    EnqueueMessage(kLogRange, spreadsheet_id_.c_str(), values);
    // oauth::AppendToSheets(kLogRange, spreadsheet_id_.c_str(), values,
                          // sheets_access_.GetAccessToken().c_str());
    // TODO: also log to file
  }

  void LogWeight(double grams, time_t log_time = 0) {
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
    sprintf(values, values_format, ctime(&tm.tv_sec), tm.tv_sec, tm.tv_nsec, grams);
    EnqueueMessage(kWeightRange, spreadsheet_id_.c_str(), values);
    // oauth::AppendToSheets(kWeightRange, spreadsheet_id_.c_str(),
                          // values, sheets_access_.GetAccessToken().c_str());
  }

};
