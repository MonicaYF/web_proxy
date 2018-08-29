#include <time.h>
#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <iterator>


class value_t{
 public:
  bool needRevalidation;
  time_t expiretime;
  std::string ETag;
  std::vector<char> request;
  // std::vector<std::vector<char>,std::vector<char> > response;
  std::vector<std::vector<char> > response;
 value_t(bool nReVal, time_t exp, std::string Et, std::vector<char> requ, std::vector<std::vector<char> > resp) :needRevalidation(nReVal), expiretime(exp), ETag(Et),  request(requ), response(resp){}
};

class Cache_t {
  
 public:
  size_t capacity;
  std::map<std::string, value_t*> Map;

  Cache_t(int cap) {
    capacity = cap;
    Map = std::map<std::string, value_t*>();
  }


  bool whetherInCache(std::string key) {
    if(Map.find(key)!=Map.end()) {
      return true;
    }
    else return false;
  }

  std::string put(std::string key, value_t *value) {
    if(Map.find(key)!= Map.end()) {

      Map[key] -> needRevalidation = value -> needRevalidation;
      Map[key] -> expiretime = value -> expiretime;
      Map[key] -> ETag = value -> ETag;
      Map[key] -> request = value -> request;
      Map[key] -> response = value -> response;
      //      delete value;
      return "00";
    }
    else{
      if ( Map.size() == capacity) {
	std::map<std::string,value_t *> :: iterator it = Map.begin();
	Map.erase(it);
	return key;
      }
      Map[key] = value;
      return "00";
    }
  }
};
