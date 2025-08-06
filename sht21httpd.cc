//g++ getsht21.cc -lmicrohttpd -lpthread -std=c++11

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>

#include <microhttpd.h>
#include "i2c.hh"


using std::uint32_t;

const uint32_t port_httpd = 80;
const std::vector<uint32_t> gpio{17, 27, 22};
std::atomic<float> *tp, *hm;


static MHD_Result
answer_to_connection (void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method,
                      const char *version, const char *upload_data,
                      size_t *upload_data_size, void **con_cls)
{
  const std::string page_begin("<html><head><meta http-equiv=\"refresh\" content=\"2\"></head><body>");
  const std::string page_end("</body></html>");
  std::string *page_p = new std::string;
  *con_cls = page_p;
  std::string &page = *page_p;
  struct MHD_Response *response;
  MHD_Result ret;
  page.clear();
  page+=page_begin;
  page+="Temperature ";
  for(uint32_t i=0; i<gpio.size(); i++){
    std::string val = std::to_string(tp[i]);
    val.resize(4);
    page+= val;
    page+=" ";
  }
  page+="  Humidity ";
  for(uint32_t i=0; i<gpio.size(); i++){
    std::string val = std::to_string(hm[i]);
    val.resize(4);
    page+= val;
    page+=" ";
  }
  page+=page_end;
  response = MHD_create_response_from_buffer (page.size(), (void *)page.c_str(),
					      MHD_RESPMEM_PERSISTENT);
  ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
  MHD_destroy_response (response);
  return ret;
}


static void
request_completed (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe){
  std::string *page_p = static_cast<std::string*>(*con_cls);
  delete page_p;
  *con_cls = 0;
}


void read_sht21(std::atomic_uchar &run){
  run = 1;

  const std::string export_str("/sys/class/gpio/export");
  const std::string unexport_str("/sys/class/gpio/unexport");
  const std::string gpio_str("/sys/class/gpio/gpio");
  for(uint32_t i=0; i<gpio.size(); i++){
    std::ofstream of_exp(export_str);
    of_exp << gpio[i];
  }
  std::this_thread::sleep_for (std::chrono::seconds(4));
  for(uint32_t i=0; i<gpio.size(); i++){
    std::ofstream of_dir(gpio_str+std::to_string(gpio[i])+"/direction");
    of_dir << "out";
    of_dir.close();
    std::this_thread::sleep_for (std::chrono::seconds(1));
    std::ofstream of_val(gpio_str+std::to_string(gpio[i])+"/value");
    if(i == 0)
      of_val << "1";
    else
      of_val << "0";
  }

  // std::this_thread::sleep_for (std::chrono::seconds(2));
  const std::string i2cdev("/dev/i2c-1"); //NOTE: old version raspi is i2c-0
  I2C_Open(i2cdev.c_str());
  I2C_Setup(I2C_SLAVE, 0x40);

  if(I2C_GetError()){	
    I2C_PrintError();
    exit(1);
  }

  uint32_t n = 0;
  uint32_t i = 0;  
  while(run){
    
    std::ofstream of_gpio(gpio_str+std::to_string(gpio[i])+"/value");
    of_gpio << "1";
    of_gpio.close();
    std::this_thread::sleep_for (std::chrono::seconds(2));
   
    std::int16_t t;
    std::uint8_t h;
    SHT21_Read(&t,&h);
    printf("%u\t%.1f\t%u\n", n++,((float)t)/10, h);
    if(!SHT21_GetError()){
      tp[i] = ((float)t)/10;
      hm[i] = h;
    }
    else{
      I2C_PrintError();
      SHT21_PrintError();
      I2C_ClearError();
    }

    std::ofstream of_gpio_xx(gpio_str+std::to_string(gpio[i])+"/value");
    of_gpio_xx << "0";
    of_gpio_xx.close();
    i++;
    if(i==gpio.size()) i=0;
  }
  I2C_Close();
}


int main (){
  std::atomic<float> tpv[gpio.size()];
  std::atomic<float> hmv[gpio.size()];
  tp = tpv;
  hm = hmv;
  
  struct MHD_Daemon *daemon;
  daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, port_httpd, NULL, NULL,
                             &answer_to_connection, NULL,
			     MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                             NULL, MHD_OPTION_END);
  if (NULL == daemon)
    return 1;

  std::atomic_uchar thread_run;
  std::thread reader(read_sht21, std::ref(thread_run));

  (void) getchar();
  thread_run = 0;
  reader.join();
  
  MHD_stop_daemon(daemon);
  return 0;
}
