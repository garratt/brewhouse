#include <cstdio>
#include <iostream>
#include <fstream>
#include "../third_party/libtwitcurl/twitcurl.h"
#include <thread>
#include <stdio.h>
#include <unistd.h>     // for read
#include <deque>
#include <mutex>

struct TwitterTokens {
  std::string consumer_key, consumer_secret;
  std::string access_key, access_secret;
  static constexpr const char * kDefaultFilename = "twitter_tokens.txt";

  void Save(const char *filename = kDefaultFilename) {
    FILE *fp = fopen(filename, "w+");
    fprintf(fp, "%s\n%s\n%s\n%s\n", consumer_key.c_str(), consumer_secret.c_str(),
            access_key.c_str(), access_secret.c_str());
    fclose(fp);
  }

  void Load(const char *filename = kDefaultFilename) {
    char ck[500], cs[500], ak[500], as[500];
    FILE *fp = fopen(filename, "rb");
    if (!fp) return;
    fscanf(fp, "%s\n%s\n%s\n%s", ck, cs, ak, as);
    fclose(fp);
    consumer_key = ck;
    consumer_secret = cs;
    access_key = ak;
    access_secret = as;
    printf("consumer key: %s\nconsumer secret: %s\naccess key: %s\naccess secret: %s\n", consumer_key.c_str(), consumer_secret.c_str(),
            access_key.c_str(), access_secret.c_str());
  }
};


class BrewTweeter {
  TwitterTokens tokens_;
  std::deque<std::string> message_queue_;
  bool quit_threads_ = false;
  std::mutex message_lock_;
  twitCurl twitterObj_;
  std::string PopMessage() {
     std::lock_guard<std::mutex> lock(message_lock_);
     if (message_queue_.size() == 0) {
       return "";
     }
     std::string current_message = message_queue_.front();
     message_queue_.pop_front();
     return current_message;
  }


  void SendMessages() {
    std::string current_message = "";
    while(true) {
      current_message = PopMessage();
      while (current_message.size() > 0) {
        std::string replyMsg;
        if(twitterObj_.statusUpdate(current_message.c_str())) {
          twitterObj_.getLastWebResponse( replyMsg );
          printf( "\ntwitterClient:: twitCurl::statusUpdate web response:\n%s\n", replyMsg.c_str() );
        } else {
          twitterObj_.getLastCurlError( replyMsg );
          printf( "\ntwitterClient:: twitCurl::statusUpdate error:\n%s\n", replyMsg.c_str() );
        }
        // printf("failed to send message %s\n");
        // }
      current_message = PopMessage();
    }
     if (quit_threads_) break;
     sleep(1);
    }
  }


  std::thread message_thread_;
  public:
  BrewTweeter() {
    tokens_.Load();
        std::string replyMsg;
    twitterObj_.getOAuth().setConsumerKey(tokens_.consumer_key);
    twitterObj_.getOAuth().setConsumerSecret(tokens_.consumer_secret);
    twitterObj_.getOAuth().setOAuthTokenKey(tokens_.access_key);
    twitterObj_.getOAuth().setOAuthTokenSecret(tokens_.access_secret);
    if( twitterObj_.accountVerifyCredGet() ) {
        twitterObj_.getLastWebResponse( replyMsg );
        printf( "\ntwitterClient:: twitCurl::accountVerifyCredGet web response:\n%s\n", replyMsg.c_str() );
    } else {
        twitterObj_.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::accountVerifyCredGet error:\n%s\n", replyMsg.c_str() );
    }
    // because std::thread is movable, just assign another one there:
    message_thread_ = std::thread(&BrewTweeter::SendMessages, this);
  }

  ~BrewTweeter() {
    quit_threads_ = true;
    if(message_thread_.joinable()) message_thread_.join();
  }
  
  void Tweet(std::string message) {
     std::lock_guard<std::mutex> lock(message_lock_);
     message_queue_.push_back(message);
  }


};
