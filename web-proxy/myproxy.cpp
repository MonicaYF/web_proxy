#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <thread>
#include <sys/time.h>
#include <cstring>
#include <vector>
#include <arpa/inet.h>
#include <sstream>
#include <tuple>
#include <time.h>
#include <mutex>
#include <fstream>
#include "Cache.h"



#define MAX_BUFFER_SIZE 1024

using namespace std;

fstream fs;

Cache_t Cache(1000);
mutex mtx;
mutex mtx_log;


void writelog ( string s ) {
  fs << s << endl;
  fs.flush();
}


int ErrorExit( string error_msg ) {
  cerr << error_msg << endl;
  return -1;
}

//_______________________________________________________

//        HttpRequest Class

//________________________________________________________

class HttpRequest {  
public:
  vector<char> req_header;
  string firstline;
  string method;
  string path;
  string protocal;
  string host;
  string port;
  size_t content_len;
  vector<char> parsed_header;
  
  HttpRequest(): content_len(0){}
  
  
  int GetHeader ( int socket_fd ) {
    cout << "__________________GetHeader " << socket_fd << "_________________" << endl;
    req_header.resize(1,'\0');
    int len = 0;
    int recv_bytes;
    while( strstr(req_header.data(),"\r\n\r\n") == NULL ) {
      recv_bytes = recv(socket_fd, &req_header.data()[len], 1, 0);
      len++;
      req_header.resize(len+1,'\0');
      if ( recv_bytes < 0 ) {
	ErrorExit("Cannot recieve request from client");
       }
    }
    req_header.resize(len);
    cout << req_header.data() << endl;
    cout << "thread with sicket: " << socket_fd << "exit get request header" << endl;
    return 0;
  }
  
  
  void parseFirstline(string msg){
    size_t n = msg.find("\r\n");
    size_t temp;
    if ( n == std::string::npos) {
      ErrorExit("Invalid massage: cannot parse");
    }
    firstline = msg.substr(0,n);
    n = firstline.find(' ');
    method = firstline.substr(0,n);
    temp = n;
    n = firstline.find(' ',temp+1);   
    path.assign(firstline, temp+1, n-temp);
    protocal = firstline.substr(n+1);
    n = msg.find("Host: ");
    if ( n != string::npos ) {
      temp = msg.find("\r\n",n);
      string hostinfo(msg,n,temp-n);
      n = hostinfo.find(' ');
      temp = hostinfo.find(':',n);
      if ( temp == std::string::npos) {
	host = hostinfo.substr(n+1);
	port = "80";
      }
      else {
	host = hostinfo.substr(n+1,temp-n-1);
	port = hostinfo.substr(temp+1);
      }
    }
    else {
      n = path.find(':');
      if ( n != string::npos) {
	host = path.substr(0,n);
	port = path.substr(n+1);
      }
    }
  }

  void parseHeader( char * h) {
    string header(h);
    size_t n1 = header.find("Content-Length");
    size_t n2;
    if ( n1 != string::npos) {
      n1 = header.find(' ',n1);
      n2 = header.find("\r\n",n1);
      string len = header.substr(n1+1,n2-n1-1);
      stringstream num(len);
      num >> content_len;
    }
  }

  int evaluateHeader ( ) {
    //invalid method
    if ( method != "GET" && method != "POST" && method != "CONNECT" ) {
      return -1;
    }
    //invalid protocal
    if ( protocal != "HTTP/1.1" && protocal != "HTTP/1.0" ) {
      return -1;
    }
    //no path
    if ( path == "" ) {
      return -1;
    }
    //no host
    if ( host == "" ) {
      return -1;
    }
    //wrong portnumber
    stringstream ss(port);
    int port_num;
    ss >> port_num;
    if ( port_num  < 0 || port_num > 65535 ) {
      return -1;
    }
    
    return 0;
  }
  
 void dealwithHeader(string msg ) {
   size_t n = path.find(host);
   if  ( n != string :: npos ){
     string subpath = path.substr(n+host.size());
     n = subpath.find(':');
     if ( n != string :: npos ) {
       subpath.erase(n,subpath.size()-n);
     }
     n = msg.find("Host");
     size_t n2 = msg.find("\r\n",n);
     string subline = msg.substr(n,n2-n);
     string subhead = method + ' ' + subpath + protocal +"\r\n" + subline + "\r\n\r\n";
     parsed_header.resize(subhead.size(),'\0');
     memcpy(&parsed_header.data()[0],subhead.c_str(),subhead.size());
   }
 }
    
  
  ~HttpRequest(){}    
};


//____________________________________________________________________________

//                    HttpResponse Class

//_____________________________________________________________________________

class HttpResponse {
public:
  vector<char> resp_header;
  vector<char> resp_cont;
  vector<char> resp_all;
  size_t content_len;
  string transfer;
  string type;
  string firstline;

  
  HttpResponse() :  resp_cont(MAX_BUFFER_SIZE*10,'0'), content_len(0), transfer(""){}

    
  int GetHeader ( int socket_fd ) {
    cout << "__________________GetHeader " << socket_fd << "_________________" << endl;
    resp_header.resize(1,'\0');
    int len = 0;
    int recv_bytes;
    while( strstr(resp_header.data(),"\r\n\r\n") == NULL ) {
      recv_bytes = recv(socket_fd, &resp_header.data()[len], 1, 0);
      len++;
      resp_header.resize(len+1,'\0');
      if ( recv_bytes < 0 ) {
	ErrorExit("Cannot recieve request from client");
      }
    }
    resp_header.resize(len);
    cout << resp_header.data() << endl;
    cout << "thread with sicket: " << socket_fd << "exit get request header" << endl;
    return 0;
  }
  
  
  void parseHeader( char * h) {
    string header(h);
    size_t n1 = header.find("\r\n");
    size_t n2;
    if ( n1 == std::string::npos) {
      ErrorExit("Invalid massage: cannot parse");
    }
    firstline = header.substr(0,n1);
    n1 = header.find("Content-Length");
    if ( n1 != string::npos) {
      n1 = header.find(' ',n1);
      n2 = header.find("\r\n",n1);
      string len = header.substr(n1+1,n2-n1-1);
      stringstream num(len);
      num >> content_len;
    }
    n1 = header.find("Transfer_Encoding: ");
    n2 = header.find("identity", n1);
    if ( n2 == string::npos ) {
      transfer = "chunked";
    }
    n1 = header.find(' ');
    n2 = header.find(' ',n1+1);
    type = header.substr(n1+1,n2-n1-1);
    
    n1 = header.find("chunked");
    if ( n1 != string::npos) {
      n2 = header.find("\r\n",n1);
      transfer = header.substr(n1,n2-n1);
    } 
  }

  void getContent ( int server_socket ) {
    resp_cont.resize(content_len);
    int length = content_len;
    while ( length > 0 ) {
      if ( length < MAX_BUFFER_SIZE ) {
 	recv(server_socket, &resp_cont.data()[content_len - length], length, 0);
	length = 0;
      }
      else {
	recv(server_socket, &resp_cont.data()[content_len - length], MAX_BUFFER_SIZE, 0);
	length -= MAX_BUFFER_SIZE;
      }
    }
  }

  
  void sendContent ( int client_socket ) {
    int length = content_len;
    while ( length > 0 ) {
      if ( length < MAX_BUFFER_SIZE ) {
	send(client_socket, &resp_cont.data()[content_len - length], length, 0);
	length = 0;
      }
      else {
	send(client_socket, &resp_cont.data()[content_len - length], MAX_BUFFER_SIZE, 0);
	length -= MAX_BUFFER_SIZE;
      }
    }
  }

  
  void cacheContent ( vector<char> & content, int recv_bytes ) {
    cout << "******************caching**********************" << endl;
    if ( (content_len + recv_bytes) < resp_cont.size() ) {
      memcpy ( &resp_cont.data()[content_len], content.data(), recv_bytes );
      content_len += recv_bytes;
      cout << content_len << " : " << resp_cont.size()  << endl;
    }
    else {
      resp_cont.resize(resp_cont.size() + (MAX_BUFFER_SIZE * 10));
      memcpy ( &resp_cont.data()[content_len], content.data(), recv_bytes );
      content_len += recv_bytes;
      cout << "buffer length :" << resp_cont.size() << "content length " << content_len << endl;
    }
  }

  vector<vector<char> > getResponseall() {
    vector<vector<char> > resp(2);
    resp[0] = resp_header;
    resp[1] = resp_cont;
    return resp;
  }
  
  ~HttpResponse() {}

};



//__________________________________________________________________________

//                 parse header for cache implementation

//__________________________________________________________________________


void skip_space(std::string & str) {
  int i = 0;
  while (str[i] == ' ') {
    str = str.substr(1);
    i++;
  }
}

time_t parseTime(std::string &expires_str) {
  time_t expiretime;

  int weekday;
  int month;
  int date;
  int year;
  int second;
  int minute;
  int hour;

  skip_space(expires_str);
  std::string weekday_str = expires_str.substr(0,3);

  if (weekday_str == "Mon") weekday = 1;
  else if (weekday_str == "Tue")weekday = 2;
  else if (weekday_str == "Wed") weekday = 3;
  else if (weekday_str == "Thu") weekday = 4;
  else if (weekday_str == "Fri") weekday = 5;
  else if (weekday_str == "Sat") weekday = 6;
  else if (weekday_str == "Sun") weekday = 0;

  size_t found = expires_str.find(",");
  expires_str = expires_str.substr(found + 1);
  skip_space(expires_str);

  found = expires_str.find(" ");
  std::string date_str = expires_str.substr(0,found);
  expires_str = expires_str.substr(found + 1);
  skip_space(expires_str);
  date = std::atoi(date_str.c_str());

  found = expires_str.find(" ");
  std::string month_str = expires_str.substr(0,found);
  expires_str = expires_str.substr(found + 1);
  skip_space(expires_str);
  if (month_str == "Jan") month = 0;
  else if (month_str == "Feb") month = 1;
  else if (month_str == "Mar") month = 2;
  else if (month_str == "Apr") month = 3;
  else if (month_str == "May") month = 4;
  else if (month_str == "Jun") month = 5;
  else if (month_str == "Jul") month = 6;
  else if (month_str == "Aug") month = 7;
  else if (month_str == "Sep") month = 8;
  else if (month_str == "Oct") month = 9;
  else if (month_str == "Nov") month = 10;
  else if (month_str == "Dec") month = 11;

  found = expires_str.find(" ");
  std::string year_str = expires_str.substr(0,found);
  expires_str = expires_str.substr(found + 1);
  skip_space(expires_str);
  year = std::atoi(year_str.c_str()) - 1900;

  found = expires_str.find(":");
  std::string hour_str = expires_str.substr(0,found);
  expires_str = expires_str.substr(found + 1);
  skip_space(expires_str);
  hour = std::atoi(hour_str.c_str());

  found = expires_str.find(":");
  std::string minute_str = expires_str.substr(0,found);
  expires_str = expires_str.substr(found + 1);
  skip_space(expires_str);
  minute = std::atoi(minute_str.c_str());

  found = expires_str.find(":");
  std::string second_str = expires_str.substr(0,found);
  expires_str = expires_str.substr(found + 1);
  skip_space(expires_str);
  second = std::atoi(second_str.c_str());


  struct tm *tm;
  time_t rawtime;
  tm = localtime(&rawtime);

  tm -> tm_wday = weekday;
  tm -> tm_mday = date;
  tm -> tm_sec = second;
  tm -> tm_min = minute;
  tm -> tm_hour = hour;
  tm -> tm_mon = month;
  tm -> tm_year = year;
  expiretime = mktime(tm);
  return expiretime;
}


void parse_response(std::string header, bool & no_store, bool & needRevalidation, time_t & expiretime, std::string & ETag,time_t now) {
  if ( header.find("no-store") != std::string::npos ){
      no_store = true;
    }
  else no_store = false;
      
  if(header.find("Need-Revalidation") != std::string::npos || header.find("no-cache") != std::string::npos){
    needRevalidation = true;
  }
  else needRevalidation = false;


  if (header.find("ETag") != std::string::npos){
    size_t ETag_begin = header.find("ETag");
    size_t ETag_end = header.find("\r\n");
    std::string ETag_str = header.substr(ETag_begin,ETag_end - ETag_begin);
    size_t E_begin = ETag_str.find("\"");
    std::string remain = ETag_str.substr(E_begin + 1);
    size_t E_end = remain.find("\"");
    ETag = remain.substr(0,E_end);
    //std::cout<<ETag<<'\n';
  }


  if (header.find("s-maxage") != std::string::npos){
    size_t maxage_begin = header.find("s-maxage");
    std::string max_age_begin_str = header.substr(maxage_begin + 8);
    size_t max_age_begin = max_age_begin_str.find("=") + 1;
    size_t max_age_end = max_age_begin_str.find("\r\n");
    std::string max_age_str = max_age_begin_str.substr(max_age_begin,max_age_end - max_age_begin);
    const char* max_age = max_age_str.c_str();
    int maxage = std::atoi(max_age);
    expiretime = maxage + now;
  }
  else if (header.find("max-age") != std::string::npos){
    size_t maxage_begin = header.find("max-age");
    std::string max_age_begin_str = header.substr(maxage_begin + 7);
    size_t max_age_begin = max_age_begin_str.find("=") + 1;
    size_t max_age_end = max_age_begin_str.find("\r\n");
    std::string max_age_str = max_age_begin_str.substr(max_age_begin,max_age_end - max_age_begin);
    const char* max_age = max_age_str.c_str();
    int maxage = std::atoi(max_age);
    expiretime = maxage + now;
  }
  else if(header.find("Expires") != std::string::npos) {
    size_t expiresbegin = header.find("Expires");
    std::string expires_begin_str = header.substr(expiresbegin + 7);
    size_t expires_begin = expires_begin_str.find(":") + 1;
    size_t expires_end = expires_begin_str.find("\r\n");
    std::string expires_str = expires_begin_str.substr(expires_begin,expires_end - expires_begin);
    expiretime = parseTime(expires_str);

  }
  else if (header.find("Last-Modified") != std::string::npos) {
    size_t lastmbegin = header.find("Last-Modified");
    std::string lastm_begin_str = header.substr(lastmbegin + 13);
    size_t lastm_begin = lastm_begin_str.find(":") + 1;
    size_t lastm_end = lastm_begin_str.find("\r\n");
    std::string lastm_str = lastm_begin_str.substr(lastm_begin,lastm_end - lastm_begin);

    time_t lastModifiedTime = parseTime(lastm_str);
    time_t now;
    time(&now);
    double diff = difftime(now, lastModifiedTime);
    diff = diff * 0.1;
    expiretime = now + diff;

  }
  else{

  }
}



//________________________________________________________________________________

//                  create socket for connection

//________________________________________________________________________________

 
int createServerSocket( const char * hostname, const char * port ) {
  int socket_fd;
  int status;
  struct addrinfo host_info;
  struct addrinfo * host_info_list;

  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family = AF_INET;
  host_info.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if ( status != 0 ) {
    cerr << "Error: cannot get address info for host" << endl;;
    return -1;
  }

  socket_fd = socket(host_info_list->ai_family,
			 host_info_list->ai_socktype,
			 host_info_list->ai_protocol);
  if (socket_fd == -1) {
    cout << "Error: cannot create server scoket" << endl;
    return -1;
  }

  status = connect ( socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if ( status == -1 ) {
    cerr << "Error: cannot connect to server" << endl;
    return -1;
  }
  
  freeaddrinfo(host_info_list);  
  return socket_fd;
}
  
int createClientSocket( int port) {
  int status;
  int socket_fd;
  struct sockaddr_in socket_addr;
  socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  socket_addr.sin_family = AF_INET;
  socket_addr.sin_addr.s_addr = INADDR_ANY;
  socket_addr.sin_port = htons(port);
  
  int yes = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
    close(socket_fd);
    ErrorExit("Error: cannot setsockopt");
  }
  
  status = bind(socket_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr));
  if ( status == -1 ) {
    ErrorExit("Error: cannot bind the client socket");
  }

  return socket_fd;
}


//_________________________________________________________________________________

//                  send and recv message

//____________________________________________________________________________________

void sendContent ( int client_socket , vector<char> & resp_cont ) {
  int length = resp_cont.size();
  while ( length > 0 ) {
    if ( length < MAX_BUFFER_SIZE ) {
      send(client_socket, &resp_cont.data()[resp_cont.size() - length], length, 0);
      length = 0;
    }
    else {
      send(client_socket, &resp_cont.data()[resp_cont.size() - length], MAX_BUFFER_SIZE, 0);
      length -= MAX_BUFFER_SIZE;
    }
  }
}


int Response ( int socket_recv, int socket_send ) {
  cout << "_______________GetResponse " << socket_recv << " ______________--"<< endl;
  vector<char> resp_buf(1024);
  int recv_bytes = recv(socket_recv, &resp_buf.data()[0], 1024, 0);
  if ( recv_bytes <= 0 ) {
    return 0;
  }
  send(socket_send, &resp_buf.data()[0], recv_bytes, 0);
  cout << "receving bytes from server : " << recv_bytes << endl;
  cout << "thread with socket: " << socket_recv << " exit" << endl;
  return 1;
}

//__________________________________________________________________________________

//                         Getmethod

//__________________________________________________________________________________

vector<vector<char> > Getmethod ( HttpRequest & request , int client_socket, int id) {
  cout << "________________getmethod" << client_socket << "_______________" << endl;
  //create a socket to connect with server
  int server_socket = createServerSocket(request.host.c_str(), request.port.c_str());
  //can not connect to server, error in request
  if ( server_socket == -1 ) {
    string msg("HTTP/1.1 400 Bad Request\r\n\r\n");
    send(client_socket, msg.c_str(), msg.size(), 0);
    close(client_socket);
    return {};
  }
  //send request to server
  cout << "************sending request to server*************" << endl;
  string log = to_string(id) + ": Requesting \"" + request.firstline + "\" from " + request.host;
  mtx_log.lock();
  writelog(log);
  mtx_log.unlock();
 
  // send(server_socket, &request.parsed_header.data()[0], request.parsed_header.size(), 0);
  //cout << request.parsed_header.data() << endl;
  send(server_socket, &request.req_header.data()[0], request.req_header.size(), 0);
  cout << request.req_header.data() << endl;
  
  //recieve response
  cout << "***********recieve response header from server************" << endl;
  HttpResponse response;
  if ( response.GetHeader(server_socket) == -1 ) {
    close(client_socket);
    close(server_socket);
    return {};
  }
  cout << " ***********resp header********\n" << response.resp_header.data() << endl;
  response.parseHeader(response.resp_header.data());
  cout << "content length: " << response.content_len << " type :" << response.type << endl;
  
  log = to_string(id) + ": Recieved \"" + response.firstline + "\" from " + request.host;
  mtx_log.lock();
  writelog(log);
  mtx_log.unlock();
    
  send(client_socket, response.resp_header.data(), response.resp_header.size(), 0);

  cout << "***************get response content and send to client ****************" << endl;

  if ( response.content_len != 0) {
    response.getContent(server_socket);
    string log = to_string(id) + ": " + "Responding " + response.firstline;
    mtx_log.lock();
    writelog(log);
    mtx_log.unlock();
    response.sendContent(client_socket);
  }

  else {
    string log = to_string(id) + ": " + "Responding " + response.firstline;
    mtx_log.lock();
    writelog(log);
    mtx_log.unlock();

    vector<char> resp_buf(MAX_BUFFER_SIZE*3 ,'\0');
    int recv_bytes = 0;
    response.content_len = 0;
    while ( strstr(resp_buf.data(),"0\r\n\r\n") == NULL ) {
      resp_buf.clear();
      resp_buf.resize(MAX_BUFFER_SIZE *3,'\0');
      recv_bytes = recv(server_socket, &resp_buf.data()[0], MAX_BUFFER_SIZE *3, 0);
      cout << resp_buf.data() << endl;
      if ( recv_bytes == 0 ) {
	break;
      }
      else if ( recv_bytes <  0 ) {
	cout << "error in recieving chuncked data" << endl;
	break;
      }
      cout << "sending to client" << endl;
      send(client_socket, &resp_buf.data()[0], recv_bytes, 0);
      if ( response.type == "200" ) {
      response.cacheContent(resp_buf, recv_bytes);
      }  
    }    
    response.resp_cont.resize(response.content_len);
  } 

  cout << "thread with socket: " << client_socket << " exit from get" << endl;  
  cout << "close serversocket: " << server_socket << endl;
  close(server_socket);
  cout << "close clientsocket " << client_socket << endl;
  close(client_socket);
  
  return response.getResponseall();
}

//________________________________________________________________________________

//                           Connect Method

//_________________________________________________________________________________


int Connectmethod( HttpRequest & request, int client_socket, int id) {
  cout << "____________________connectmethod " << client_socket << "___________________" <<endl;
  //create a socket to connect with server
  int server_socket = createServerSocket(request.host.c_str(), request.port.c_str());
  if ( server_socket == -1 ) {
    string msg("HTTP/1.1 400 Bad Request\r\n\r\n");
    send(client_socket, msg.c_str(), msg.size(), 0);
    close(client_socket);
    return -1;
  }
  string resp = "HTTP/1.1 200 OK\r\n\r\n";
  cout << "**************sent 200 ok********************" << endl;
  send(client_socket, resp.c_str(), resp.size(), 0);

  int max_sd,activity;
  fd_set readfds;
  
  while(1) {
    FD_ZERO(&readfds);
    FD_SET( server_socket, &readfds);
    FD_SET ( client_socket, &readfds);
    if ( client_socket > server_socket ) {
      max_sd = client_socket;
    }
    else {
      max_sd = server_socket;
    }
    activity = select( max_sd +1, &readfds, NULL, NULL, NULL);
    cout << "activity: " << activity << endl;
    
    if ( activity < 0 ) {
      ErrorExit("select error");
    }
    else if ( activity > 0 ) {
      if ( FD_ISSET(client_socket, &readfds)) {
	if ( (Response(client_socket, server_socket) ) ==  0){
	  close(server_socket);
	  close(client_socket);
	  cout << "close connection from client" << endl; 
	  break;
	}
      }
      if ( FD_ISSET(server_socket, &readfds) ) {
	if ( (Response(server_socket, client_socket) ) == 0 ) {
	  close(server_socket);
	  close(client_socket);
 	  cout << "close connection from sever" << endl;
	  break;
	}	
      }
    }
  }

  string log = to_string(id) + ": " + "Tunnel closed";
  mtx_log.lock();
  writelog(log);
  mtx_log.unlock();

  cout << "thread with socket: " << client_socket << " eixt connect" << endl;
  return 0;
}


//___________________________________________________________________________________

//                   Post Method

//____________________________________________________________________________________


int Postmethod ( HttpRequest & request, int client_socket ) {
  cout << "________________getmethod" << client_socket << "_______________" << endl;
  //create a socket to connect with server
  int server_socket = createServerSocket(request.host.c_str(), request.port.c_str());
  if ( server_socket == -1 ) {
    string msg("HTTP/1.1 400 Bad Request\r\n\r\n");
    send(client_socket, msg.c_str(), msg.size(), 0);
    close(client_socket);
    return -1;
  } 
  //send request to server
  cout << "************sending request to server*************" << endl;
  send(server_socket, &request.req_header.data()[0], request.req_header.size(), 0);
  cout << &request.req_header.data()[0] << endl;

  request.parseHeader(request.req_header.data());
  cout << "content length: " << request.content_len << endl;
  if ( request.content_len != 0) { 
    vector<char> post_buf(request.content_len,'0');
    recv(client_socket, &post_buf.data()[0], request.content_len, 0);
    cout << "**********sending post content to server****************" << endl;
    send(server_socket, &post_buf.data()[0], request.content_len, 0);
  }
  else {
    string msg("HTTP/1.1 400 Bad Request\r\n\r\n");
    send(client_socket, msg.c_str(), msg.size(), 0);
    close(client_socket);
    return -1;
  } 
  close(client_socket);
  close(server_socket);
  cout << "thread with socket: " << client_socket << " exit post" << endl;
  return 0;
}

//_____________________________________________________________________________________

//    Deal with resquest header, according to the message from header to find whether
//    the message is in cache or not

//______________________________________________________________________________________


//need, ETag, key, response(header+content), request header)
tuple<bool, string, string, vector<vector<char> >, HttpRequest > dealwithRequest( HttpRequest & request , int id) {

  cout << "***************************deal with request**********************" << endl;
  bool need = false;
  string ETag("00");
  string key (request.path);
  vector<vector<char> > response_all;

  tuple <bool, string, string, vector<vector<char> >, HttpRequest >ans;

  if (Cache.Map.find(key) != Cache.Map.end()) {
    if (Cache.Map[key] -> needRevalidation) {
      if (Cache.Map[key] -> ETag != "00") {
	ETag = Cache.Map[key] -> ETag;
      }
      string log = to_string(id) + ": in cache, requires validation";
      mtx_log.lock();
      writelog(log);
      mtx_log.unlock();
      cout << "needrevalidation and append etag" << endl;
      need = true;
      response_all = {};
    }
    else {
      time_t now;
      time(&now);
      double diff = difftime(now, Cache.Map[key] -> expiretime);
      if (diff > 0) {
	string log = to_string(id) + ": in cache, valid";
	mtx_log.lock();
	writelog(log);
	mtx_log.unlock();	 
	response_all = Cache.Map[key] -> response;
      }
      else {
	string exptime(ctime(&(Cache.Map[key]->expiretime)));
	string log = to_string(id) + ": in cache, but expired at " + exptime;
	mtx_log.lock();
	writelog(log);
	mtx_log.unlock();
	cout << "timeout" << endl;
	need = true;
	response_all = {};
      }
    }
  }
  else {
    string log = to_string(id) + ": not in cache";
    mtx_log.lock();
    writelog(log);
    mtx_log.unlock();
    
    cout << "not in cache " << endl;
    need = true;
    response_all = {};
  }
  ans = make_tuple(need, ETag, key, response_all, request);
  return ans;
}


//____________________________________________________________________________________

//      According to the information from decide to get response from server or cache

//_________________________________________________________________________________________


//response_all, request_header, path, isnew,// need, ETag, ket, response_all, request
tuple<vector<vector<char> >,vector<char>,string,bool> dealwithReturn(tuple<bool,string,string,vector<vector<char> > ,HttpRequest > & ans ,int client_socket,int id ) {
  cout << "************************** dealwithreturn***************************" << endl;

  tuple<vector<vector<char> >, vector<char>, string, bool> mytuple;

  vector<vector<char> > response_all;
  string key = get<2>(ans);
  bool isnew;
  string ETag;
  HttpRequest request = get<4>(ans);
    
  // in cache, have ETag, need revalidation
  if (get<1>(ans) != "00") {
    ETag = get<1>(ans);
    string s("If-None-Match: ");
    s += ETag;
    s += "\r\n\r\n";
    cout << s << endl;
    //send request + ETag to server to get new response 
    int len = request.req_header.size();
    request.req_header.resize(s.size()+len);
    memcpy(&request.req_header.data()[len-2],s.c_str(),s.size());
    response_all = Getmethod( request, client_socket, id );
     
    if (response_all[1].empty()) {
      response_all[1] = Cache.Map[request.path]->response[1];
    }
    isnew = true;
    cout << "appending etag and resent request" << endl;
  }
  else {// no cache, send request to client and get new response
    if (get<0>(ans) == true) {
      response_all = Getmethod (request, client_socket, id );
      isnew = true;
      cout << " revalidation with old request" << endl;
    }
    else {// in cache, send response directly to client
      response_all = Cache.Map[key] -> response;
      send(client_socket, response_all[0].data(), response_all[0].size(), 0);

      string header(response_all[0].data());
      size_t n = header.find("\r\n");
      string first = header.substr(0,n);
      string log = to_string(id) + ": Responding " + first;
      mtx_log.lock();
      writelog(log);
      mtx_log.unlock();
      
      sendContent ( client_socket , response_all[1]) ;
      isnew =false;
      cout << "in cache " << endl;
    }
  }

  mytuple = std::make_tuple(response_all, request.req_header, key, isnew);
  return mytuple;
}

//__________________________________________________________________________________

//                           implement cache

//__________________________________________________________________________________



void doCache(tuple<vector<vector<char> >, vector<char>, string, bool > & mytuple , int id ) {
  cout << "*************************** do caching**************************" << endl;
  vector<vector<char> > response_all = get<0>(mytuple);
  bool needRevalidation = false;
  bool no_store = false;
  time_t now;
  time(&now); 
  time_t expiretime = now;
  string ETag = "00";
  string key = get<2>(mytuple);
  vector<char> request = get<1>(mytuple);
  if ( !response_all.empty() ) {
    string response_header(response_all[0].data());
    parse_response(response_header,no_store, needRevalidation,expiretime,ETag,now );
    if ( no_store ) {
      string log = to_string(id) + ": " + "not cacheable because there is a \"no_store\" in the response header";
      mtx_log.lock();
      writelog(log);
      mtx_log.unlock();
    }
    else {
      if ( needRevalidation ) {
	string log = to_string(id) + ": " + "cached, but requires re_validation";
	mtx_log.lock();
	writelog(log);
	mtx_log.unlock();
      }
      else {
	string expire_time(ctime(&expiretime));
	string log = to_string(id) + ": " + "cached, expires at " + expire_time;
	mtx_log.lock();
	writelog(log);
	mtx_log.unlock();
      }
    
      if ( ETag != "00" ) {
	string log = to_string(id) + ": " + "NOTE ETag: " + ETag;
	mtx_log.lock();
	writelog(log);
	mtx_log.unlock();
      }
  
      value_t * newValue = new value_t(needRevalidation, expiretime, ETag, request, response_all);
      string fromcache = Cache.put(key, newValue);
      if ( fromcache != "00" ) {
	string log = "(no-id): NOTE evicted " + fromcache + " from cache"; 
	mtx_log.lock();
	writelog(log);
	mtx_log.unlock();
      }
    }
  }
}

//________________________________________________________________________________________

//        Main function to deal with request

//_________________________________________________________________________________________

int dealwithGetmethod ( HttpRequest & request, int client_socket , int id) {
  tuple<bool, string, string, vector<vector<char> >, HttpRequest > request_status = dealwithRequest ( request, id );
  cout << "deal with return " << endl;
  tuple<vector<vector<char> >, vector<char>, string, bool> cache_status = dealwithReturn(request_status, client_socket, id);
  mtx.lock();
  doCache(cache_status, id);
  mtx.unlock();
  return 0;
}


//recieve data through multi-thread
int RequestFromClient ( char * IP, int socket_fd , int id ) {
  cout << "__________________request from guest " << socket_fd << "___________"<< endl;
  vector<char> message;
  HttpRequest request;
    
  //recieve request from client
  cout << "************recieve request from client**************" << endl;  
 if ( request.GetHeader(socket_fd) == -1 ) {
    close(socket_fd);
    return -1;
  }
  cout << "request size: " << request.req_header.size() << endl;
  cout << request.req_header.data() << endl;
  
  //parse http request
  string s (&request.req_header.data()[0]);
  request.parseFirstline(s);
  cout << request.method << endl;
  cout << request.host << endl;
  cout << request.port << endl;
  cout << request.method << endl;
  cout << request.protocal << endl;

  //write log
  time_t current;
  time(&current);
  string logTime(ctime(&current));
  string IPaddr(IP);
  string log = to_string(id)+": \"" + request.firstline + "\" from " + IPaddr + " @ " + logTime;
  mtx_log.lock();
  writelog(log);
  mtx_log.unlock();
  
  //invalidate header request 
  if ( request.evaluateHeader() != 0 ) {
    string log = to_string(id) + ": " + "ERROR Bad Request";
    mtx_log.lock();
    writelog(log);
    mtx_log.unlock();

    string msg("HTTP/1.1 400 Bad Request\r\n\r\n");
    send(socket_fd, msg.c_str(), msg.size(), 0);
    cout << "bad request and exit" << endl;
    close(socket_fd);
    return -1;
  }

  // request.dealwithHeader(s);
  
  switch(request.method[0]){
  case 'G':
    //return Getmethod(request, socket_fd);
    return dealwithGetmethod ( request, socket_fd , id) ;
  case 'C':
    return Connectmethod(request, socket_fd, id);
  case 'P':
    return Postmethod(request, socket_fd);
  }
  
  cout << "thread with socket: " << socket_fd << " exit get request" << endl;
  return 0;
}



int main( int argc, char * argv[]){
  //socket for proxy
  int status,client_socket,activity,max_sd;
  fd_set readfds;
  int id = 0;
  fs.open("/var/log/erss/proxy.log", fstream::out | fstream::app);
  
  if ( argc < 2 ) {
    ErrorExit("Please enter a port number");
  }
  
  int port = atoi(argv[1]);
  
  //create a socket to conncet with client  
  client_socket = createClientSocket(port);
  status = listen(client_socket,100);
  if ( status == -1 ) {
    close(client_socket);
    ErrorExit("Error: cannot listen");
  }
  //accecpt the connection from client
  vector<thread> threads;
  while (1) {
     FD_ZERO(&readfds);
     FD_SET( client_socket, &readfds);
     max_sd = client_socket;

     activity = select(max_sd+1, &readfds, NULL, NULL, NULL);
     if (activity < 0) {
       ErrorExit("can not select for client_socket");
     }

     if ( FD_ISSET(client_socket, &readfds)) {
       struct sockaddr socket_addr;
       socklen_t socket_addr_len = sizeof(socket_addr);
       int new_socket = accept(client_socket, &socket_addr, &socket_addr_len);
       cout << "!!!!!!!!!!!!! new connection: " << new_socket << endl;
       if ( new_socket == -1) {
	 cerr << "Error: cannot accept new conection with client" << endl;
	 continue;
       }
       struct sockaddr_in * addr_in = (struct sockaddr_in * )& socket_addr;
       char * IP = inet_ntoa(addr_in->sin_addr );
       thread(RequestFromClient, IP, new_socket, id).detach();
       ++id;
     }  
  }
  return EXIT_SUCCESS;
}



    
    
