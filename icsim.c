/*
 * Instrument cluster simulator - FULL SMOOTHED VERSION
 *
 * (c) 2014 Open Garages - Craig Smith <craig@theialabs.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "lib.h"

#ifndef DATA_DIR
#define DATA_DIR "./data/"
#endif

#define SCREEN_WIDTH 692
#define SCREEN_HEIGHT 329
#define DOOR_LOCKED 0
#define DOOR_UNLOCKED 1
#define OFF 0
#define ON 1
#define DEFAULT_DOOR_ID 411
#define DEFAULT_DOOR_BYTE 2
#define CAN_DOOR1_LOCK 1
#define CAN_DOOR2_LOCK 2 
#define CAN_DOOR3_LOCK 4
#define CAN_DOOR4_LOCK 8
#define DEFAULT_SIGNAL_ID 392
#define DEFAULT_SIGNAL_BYTE 0
#define CAN_LEFT_SIGNAL 1
#define CAN_RIGHT_SIGNAL 2
#define DEFAULT_SPEED_ID 580
#define DEFAULT_SPEED_BYTE 3

const int canfd_on = 1;
int debug = 0;
int randomize = 0;
int seed = 0;
int door_pos = DEFAULT_DOOR_BYTE;
int signal_pos = DEFAULT_SIGNAL_BYTE;
int speed_pos = DEFAULT_SPEED_BYTE;
long current_speed = 0;
float displayed_speed = 0; // TITREME COZUCU
int door_status[4];
int turn_status[2];
char *model = NULL;
char data_file[256];
SDL_Renderer *renderer = NULL;
SDL_Texture *base_texture = NULL;
SDL_Texture *needle_tex = NULL;
SDL_Texture *sprite_tex = NULL;
SDL_Rect speed_rect;

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

char *get_data(char *fname) {
  if(strlen(DATA_DIR) + strlen(fname) > 255) return NULL;
  strncpy(data_file, DATA_DIR, 255);
  strncat(data_file, fname, 255-strlen(data_file));
  return data_file;
}

void init_car_state() {
  door_status[0] = DOOR_LOCKED;
  door_status[1] = DOOR_LOCKED;
  door_status[2] = DOOR_LOCKED;
  door_status[3] = DOOR_LOCKED;
  turn_status[0] = OFF;
  turn_status[1] = OFF;
}

void blank_ic() {
  SDL_RenderCopy(renderer, base_texture, NULL, NULL);
}

void update_speed() {
  SDL_Rect dial_rect;
  SDL_Point center;
  double angle = 0;
  
  dial_rect.x = 200; dial_rect.y = 80; dial_rect.h = 130; dial_rect.w = 300;
  SDL_RenderCopy(renderer, base_texture, &dial_rect, &dial_rect);
  dial_rect.x = 250; dial_rect.y = 30; dial_rect.h = 65; dial_rect.w = 200;
  SDL_RenderCopy(renderer, base_texture, &dial_rect, &dial_rect);
  dial_rect.x = 323; dial_rect.y = 171; dial_rect.h = 52; dial_rect.w = 47;
  SDL_RenderCopy(renderer, base_texture, &dial_rect, &dial_rect);

  center.x = 135; 
  center.y = 20;
  
  angle = map((long)displayed_speed, 0, 280, 0, 180);
  if(angle < 0) angle = 0;
  if(angle > 180) angle = 180;
  
  SDL_RenderCopyEx(renderer, needle_tex, NULL, &speed_rect, angle, &center, SDL_FLIP_NONE);
}

void update_doors() {
  SDL_Rect door_area, update, pos;
  
  // 1. TEMİZLİK ALANI (Arkaplanı temizleyen bölge)
  door_area.x = 400; // Biraz daha soldan başlatıyoruz temizliği
  door_area.y = 215; 
  door_area.w = 150; 
  door_area.h = 100;
  SDL_RenderCopy(renderer, base_texture, &door_area, &door_area);

  if(door_status[0] == DOOR_LOCKED && door_status[1] == DOOR_LOCKED &&
     door_status[2] == DOOR_LOCKED && door_status[3] == DOOR_LOCKED) return;

  // 2. ANA KIRMIZI GÖVDE (Sağa fazla gitmişti, 442'ye çektim)
  update.x = 440; update.y = 239; update.w = 45; update.h = 83;
  
  memcpy(&pos, &update, sizeof(SDL_Rect));
  pos.x = 442; // ESKİSİ 460'dı, şimdi sola çektik
  pos.y = 220; 
  SDL_RenderCopy(renderer, sprite_tex, &update, &pos);
  
  // 3. TEK TEK KAPILAR (Gövdeye göre hizalı kalarak hepsi sola kaydı)
  
  // Ön Sol Kapı
  if(door_status[0] == DOOR_UNLOCKED) {
    update.x = 420; update.y = 263; update.w = 21; update.h = 22;
    memcpy(&pos, &update, sizeof(SDL_Rect));
    pos.x = 420; // Sola kaydırıldı
    pos.y = 241;
    SDL_RenderCopy(renderer, sprite_tex, &update, &pos);
  }
  
  // Ön Sağ Kapı
  if(door_status[1] == DOOR_UNLOCKED) {
    update.x = 484; update.y = 261; update.w = 21; update.h = 22;
    memcpy(&pos, &update, sizeof(SDL_Rect));
    pos.x = 484; // Sola kaydırıldı
    pos.y = 239;
    SDL_RenderCopy(renderer, sprite_tex, &update, &pos);
  }

  // Arka Sol Kapı
  if(door_status[2] == DOOR_UNLOCKED) {
    update.x = 420; update.y = 284; update.w = 21; update.h = 22;
    memcpy(&pos, &update, sizeof(SDL_Rect));
    pos.x = 420;
    pos.y = 262;
    SDL_RenderCopy(renderer, sprite_tex, &update, &pos);
  }

  // Arka Sağ Kapı
  if(door_status[3] == DOOR_UNLOCKED) {
    update.x = 484; update.y = 287; update.w = 21; update.h = 22;
    memcpy(&pos, &update, sizeof(SDL_Rect));
    pos.x = 484;
    pos.y = 265;
    SDL_RenderCopy(renderer, sprite_tex, &update, &pos);
  }
}
void update_turn_signals() {
  SDL_Rect left, right, lpos, rpos;
  
  // Sprite sayfasındaki ikonların koordinatları (Buna dokunma)
  left.x = 213; left.y = 51; left.w = 45; left.h = 45;
  memcpy(&right, &left, sizeof(SDL_Rect));
  right.x = 482;

  // --- SOL SİNYAL KONUMU (692x329 ÇÖZÜNÜRLÜĞE GÖRE) ---
  // Fotoğrafa göre sol ok yaklaşık bu koordinatlarda
  lpos.w = 40; lpos.h = 40; // Biraz küçülttüm ki yuvasına tam otursun
  lpos.x = 165; // Sağa-Sola kaydırmak için burayla oyna (Azaltırsan sola gider)
  lpos.y = 25;  // Yukarı-Aşağı kaydırmak için burayla oyna (Azaltırsan yukarı gider)

  // --- SAĞ SİNYAL KONUMU (692x329 ÇÖZÜNÜRLÜĞE GÖRE) ---
  rpos.w = 40; rpos.h = 40;
  rpos.x = 485; // Sağa-Sola kaydırmak için burayla oyna (Artırırsan sağa gider)
  rpos.y = 25;  // Yukarı-Aşağı kaydırmak için burayla oyna

  // Çizim işlemi (Arka plan temizleme dahil)
  if(turn_status[0] == OFF) {
    // Kapalıyken orijinal kadran resmini oraya tekrar bas (Temizlik)
    SDL_RenderCopy(renderer, base_texture, &lpos, &lpos);
  } else {
    // Açıkken sprite'tan ikonu bas
    SDL_RenderCopy(renderer, sprite_tex, &left, &lpos);
  }

  if(turn_status[1] == OFF) {
    SDL_RenderCopy(renderer, base_texture, &rpos, &rpos);
  } else {
    SDL_RenderCopy(renderer, sprite_tex, &right, &rpos);
  }
}

void update_speed_status(struct canfd_frame *cf, int maxdlen) {
  int len = (cf->len > maxdlen) ? maxdlen : cf->len;
  if(len < speed_pos + 1) return;
  if (model && !strncmp(model, "bmw", 3)) {
    current_speed = (((cf->data[speed_pos + 1] - 208) * 256) + cf->data[speed_pos]) / 16;
  } else {
    int speed = cf->data[speed_pos] << 8;
    speed += cf->data[speed_pos + 1];
    current_speed = (speed / 100) * 0.6213751;
  }
}

void update_signal_status(struct canfd_frame *cf, int maxdlen) {
  int len = (cf->len > maxdlen) ? maxdlen : cf->len;
  if(len < signal_pos) return;
  turn_status[0] = (cf->data[signal_pos] & CAN_LEFT_SIGNAL) ? ON : OFF;
  turn_status[1] = (cf->data[signal_pos] & CAN_RIGHT_SIGNAL) ? ON : OFF;
}

void update_door_status(struct canfd_frame *cf, int maxdlen) {
  int len = (cf->len > maxdlen) ? maxdlen : cf->len;
  if(len < door_pos) return;
  door_status[0] = (cf->data[door_pos] & CAN_DOOR1_LOCK) ? DOOR_LOCKED : DOOR_UNLOCKED;
  door_status[1] = (cf->data[door_pos] & CAN_DOOR2_LOCK) ? DOOR_LOCKED : DOOR_UNLOCKED;
  door_status[2] = (cf->data[door_pos] & CAN_DOOR3_LOCK) ? DOOR_LOCKED : DOOR_UNLOCKED;
  door_status[3] = (cf->data[door_pos] & CAN_DOOR4_LOCK) ? DOOR_LOCKED : DOOR_UNLOCKED;
}

void Usage(char *msg) {
  if(msg) printf("%s\n", msg);
  printf("Usage: icsim [options] <can>\n\t-r\trandomize IDs\n\t-s\tseed value\n\t-d\tdebug mode\n\t-m\tmodel NAME\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  int opt, can, nbytes, maxdlen;
  struct ifreq ifr; struct sockaddr_can addr; struct canfd_frame frame;
  struct iovec iov; struct msghdr msg; struct cmsghdr *cmsg;
  char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
  int running = 1; canid_t door_id, signal_id, speed_id; SDL_Event event;

  while ((opt = getopt(argc, argv, "rs:dm:h?")) != -1) {
    switch(opt) {
      case 'r': randomize = 1; break;
      case 's': seed = atoi(optarg); break;
      case 'd': debug = 1; break;
      case 'm': model = optarg; break;
      default: Usage(NULL);
    }
  }
  if (optind >= argc) Usage("Specify CAN device");

  can = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  addr.can_family = AF_CAN;
  strncpy(ifr.ifr_name, argv[optind], IFNAMSIZ-1);
  ioctl(can, SIOCGIFINDEX, &ifr);
  addr.can_ifindex = ifr.ifr_ifindex;
  setsockopt(can, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_on, sizeof(canfd_on));
  
  // Non-blocking mode
  int flags = fcntl(can, F_GETFL, 0);
  fcntl(can, F_SETFL, flags | O_NONBLOCK);

  bind(can, (struct sockaddr *)&addr, sizeof(addr));
  init_car_state();
  door_id = DEFAULT_DOOR_ID; signal_id = DEFAULT_SIGNAL_ID; speed_id = DEFAULT_SPEED_ID;

  if (randomize || seed) {
    if(randomize) seed = time(NULL);
    srand(seed);
    door_id = (rand() % 2046) + 1; signal_id = (rand() % 2046) + 1; speed_id = (rand() % 2046) + 1;
    door_pos = rand() % 9; signal_pos = rand() % 9; speed_pos = rand() % 8;
  }

  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *window = SDL_CreateWindow("IC Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  base_texture = SDL_CreateTextureFromSurface(renderer, IMG_Load(get_data("ic.png")));
  needle_tex = SDL_CreateTextureFromSurface(renderer, IMG_Load(get_data("needle.png")));
  sprite_tex = SDL_CreateTextureFromSurface(renderer, IMG_Load(get_data("spritesheet.png")));

  speed_rect.x = 212; speed_rect.y = 175;
  SDL_Surface *temp_needle = IMG_Load(get_data("needle.png"));
  speed_rect.w = temp_needle->w; speed_rect.h = temp_needle->h;
  SDL_FreeSurface(temp_needle);

  while(running) {
    while(SDL_PollEvent(&event) != 0) {
      if(event.type == SDL_QUIT) running = 0;
    }

    // CAN Trafiğini Oku
    while(read(can, &frame, sizeof(struct canfd_frame)) > 0) {
      if(frame.can_id == door_id) update_door_status(&frame, CANFD_MAX_DLEN);
      if(frame.can_id == signal_id) update_signal_status(&frame, CANFD_MAX_DLEN);
      if(frame.can_id == speed_id) update_speed_status(&frame, CANFD_MAX_DLEN);
    }

    // TİTREME ÖNLEYİCİ FİLTRE
    displayed_speed = (displayed_speed * 0.93) + (current_speed * 0.07);

    blank_ic();
    update_doors();
    update_turn_signals();
    update_speed(); // İbreyi en üste çiziyoruz
    SDL_RenderPresent(renderer);

    SDL_Delay(10); 
  }

  SDL_Quit();
  return 0;
}
