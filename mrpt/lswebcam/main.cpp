
#include <iostream>
#include <sstream>
#include <webcam.h>
#include <stdlib.h>

#define err(x) (err_(x, #x, __FILE__, __LINE__))
#define errh(x) (errh_(x, #x, __FILE__, __LINE__))

CResult err_(CResult res, char const *cmd, char const *file, int line) {
  if (res != C_SUCCESS) {
    std::cerr << file << ":" << line <<": " << cmd << ": " << res << ": " << c_get_error_text(res) << std::endl;
    exit(1);
  }
  return res;
}

CHandle errh_(CHandle h, char const *cmd, char const *file, int line) {
  if (h == 0) {
    std::cerr << file << ":" << line <<": " << cmd << ": " << h << std::endl;
    exit(1);
  }
  return h;
}


CHandle hRight;
CHandle hLeft;

void open_cameras() {
  hRight = errh(c_open_device("/dev/videoX"));
  hLeft = errh(c_open_device("/dev/videoY"));
}

std::string ctl_value(CControlType type, CControlValue const &val) {
  std::stringstream ss;
  switch (type) {
  case CC_TYPE_BOOLEAN:
    ss << "bool(" << (val.value ? "true" : "false") << ")";
    break;
  case CC_TYPE_CHOICE:
    ss << "choice(" << val.value << ")";
    break;
  case CC_TYPE_BYTE:
    ss << "byte(" << val.value << ")";
    break;
  case CC_TYPE_WORD:
    ss << "word(" << val.value << ")";
    break;
  case CC_TYPE_DWORD:
    ss << "dword(" << val.value << ")";
    break;
  default:
    ss << "unknown value type " << type;
    break;
  }
  return ss.str();
}

void configure_cameras() {
  struct {
    CDevice dev;
    char buf[1024];
  } dev;
  unsigned int size = sizeof(dev);
  err(c_get_device_info(hRight, "/dev/video0", &dev.dev, &size));
  std::cout << "==== cameras ====" << std::endl;
  std::cout << "right {" << std::endl;
  std::cout << "name: " << dev.dev.name << std::endl;
  std::cout << "shortName: " << dev.dev.shortName << std::endl;
  std::cout << "driver: " << dev.dev.driver << std::endl;
  std::cout << "location: " << dev.dev.location << std::endl;
  std::cout << "}" << std::endl;
  size = sizeof(dev);
  err(c_get_device_info(hLeft, "/dev/video1", &dev.dev, &size));
  std::cout << "left {" << std::endl;
  std::cout << "name: " << dev.dev.name << std::endl;
  std::cout << "shortName: " << dev.dev.shortName << std::endl;
  std::cout << "driver: " << dev.dev.driver << std::endl;
  std::cout << "location: " << dev.dev.location << std::endl;
  std::cout << "}" << std::endl;

  char pfBuf[4096];
  unsigned int cnt = 0;
  size = sizeof(pfBuf);
  err(c_enum_pixel_formats(hRight, (CPixelFormat *)pfBuf, &size, &cnt));
  unsigned int found = (unsigned int)-1;
  std::cout << "==== pixel formats ====" << std::endl;
  for (unsigned int i = 0; i < cnt; ++i) {
    CPixelFormat *pf = (CPixelFormat *)pfBuf + i;
    std::cout << "[" << i << "] {" << std::endl;
    std::cout << "fourcc: " << pf->fourcc << std::endl;
    std::cout << "name: " << (pf->name ? pf->name : "(none)") << std::endl;
    std::cout << "mimeType: " << (pf->mimeType ? pf->mimeType : "(none)") << std::endl;
    std::cout << "}" << std::endl;
    if (pf->fourcc == std::string("MJPG")) {
      found = i;
    }
  }
  if (found == (unsigned int)-1) {
    std::cerr << "Could not find MJPG format -- giving up!" << std::endl;
    exit(1);
  }

  char fsBuf[4096];
  size + sizeof(fsBuf);
  err(c_enum_frame_sizes(hRight, (CPixelFormat *)pfBuf + found, 
    (CFrameSize *)fsBuf, &size, &cnt));
  unsigned int sz1080 = (unsigned int)-1;
  unsigned int sz720 = (unsigned int)-1;
  std::cout << "==== frame sizes ====" << std::endl;
  for (unsigned int i = 0; i < cnt; ++i) {
    CFrameSize *fs = (CFrameSize *)fsBuf + i;
    std::cout << "[" << i << "] {" << std::endl;
    std::cout << "type: " << fs->type << std::endl;
    switch (fs->type) {
    case CF_SIZE_DISCRETE:
      std::cout << "width: " << fs->width << std::endl;
      std::cout << "height: " << fs->height << std::endl;
      if (fs->height == 1080 && fs->width == 1920) {
        sz1080 = i;
      }
      if (fs->height == 720 && fs->width == 1280) {
        sz720 = i;
      }
      break;
    case CF_SIZE_CONTINUOUS:
      std::cout << "min_width: " << fs->min_width << std::endl;
      std::cout << "max_width: " << fs->max_width << std::endl;
      std::cout << "min_height: " << fs->min_height << std::endl;
      std::cout << "max_height: " << fs->max_height << std::endl;
      if (fs->min_height <= 1080 && fs->max_height >= 1080
        && fs->min_width <= 1920 && fs->max_width >= 1920) {
        sz1080 = i;
      }
      if (fs->min_height <= 720 && fs->max_height >= 720
        && fs->min_width <= 1280 && fs->max_width >= 1280) {
        sz720 = i;
      }
      break;
    case CF_SIZE_STEPWISE:
      std::cout << "min_width: " << fs->min_width << std::endl;
      std::cout << "max_width: " << fs->max_width << std::endl;
      std::cout << "step_width: " << fs->step_width << std::endl;
      std::cout << "min_height: " << fs->min_height << std::endl;
      std::cout << "max_height: " << fs->max_height << std::endl;
      std::cout << "step_height: " << fs->step_height << std::endl;
      if (fs->min_height <= 1080 && fs->max_height >= 1080 && 
        (1080 - fs->min_height) % fs->step_height == 0 &&
        fs->min_width <= 1920 && fs->max_width >= 1920 &&
        (1920 - fs->min_width) % fs->step_width == 0) {
        sz1080 = i;
      }
      if (fs->min_height <= 720 && fs->max_height >= 720 && 
        (720 - fs->min_height) % fs->step_height == 0 &&
        fs->min_width <= 1280 && fs->max_width >= 1280 &&
        (1280 - fs->min_width) % fs->step_width == 0) {
        sz720 = i;
      }
      break;
    default:
      std::cerr << "error: unknown type" << std::endl;
      break;
    }
    std::cerr << "}" << std::endl;
  }
  if (sz1080 == (unsigned int)-1 && sz720 == (unsigned int)-1) {
    std::cerr << "Cannot find HD formats -- giving up!" << std::endl;
    exit(1);
  }

  unsigned int selected = sz720;
  if (sz720 == (unsigned int)-1) {
    std::cerr << "Cannot find 720p HD formats -- giving up!" << std::endl;
    exit(1);
  }
  char frBuf[4096];
  size = sizeof(frBuf);
  err(c_enum_frame_intervals(hRight, (CPixelFormat *)pfBuf + found,
    (CFrameSize *)fsBuf + sz720, (CFrameInterval *)frBuf, &size, &cnt));
  unsigned int hz10 = (unsigned int)-1;
  std::cout << "==== frame intervals ===" << std::endl;
  for (unsigned int i = 0; i < cnt; ++i) {
    CFrameInterval *fi = (CFrameInterval *)frBuf + i;
    std::cout << "[" << i << "] {" << std::endl;
    std::cout << "type: " << fi->type << std::endl;
    switch (fi->type) {
      case CF_INTERVAL_DISCRETE:
        std::cout << "n: " << fi->n << std::endl;
        std::cout << "d: " << fi->d << std::endl;
        if (fi->d == 10 * fi->n) {
          hz10 = i;
        }
        break;
      case CF_INTERVAL_STEPWISE:
      case CF_INTERVAL_CONTINUOUS:
        std::cout << "min_n: " << fi->min_n << std::endl;
        std::cout << "max_n: " << fi->min_n << std::endl;
        std::cout << "min_d: " << fi->min_n << std::endl;
        std::cout << "max_d: " << fi->min_n << std::endl;
        std::cout << "step_n: " << fi->step_n << std::endl;
        std::cout << "step_d: " << fi->step_d << std::endl;
        if (fi->max_n * 10 > fi->min_d && fi->min_n * 10 < fi->max_d) {
          //  maybe I can find a format here?
        }
        break;
    }
    std::cout << "}" << std::endl;
  }
  if (hz10 == (unsigned int)-1) {
    std::cerr << "Can't find 10Hz frame rate -- giving up!" << std::endl;
    exit(1);
  }
  
  char cpBuf[16384];
  size = sizeof(cpBuf);
  err(c_enum_controls(hRight, (CControl *)cpBuf, &size, &cnt));
  std::cout << "==== config params ===" << std::endl;
  for (unsigned int i = 0; i < cnt; ++i) {
    CControl *cc = (CControl *)cpBuf + i;
    std::cout << "[" << i << "] {" << std::endl;
    std::cout << "id: " << cc->id << std::endl;
    std::cout << "name: " << cc->name << std::endl;
    std::cout << "type: " << cc->type << std::endl;
    std::cout << "flags: " << cc->flags << std::endl;
    std::cout << "value: " << ctl_value(cc->type, cc->value) << std::endl;
    std::cout << "def: " << ctl_value(cc->type, cc->def) << std::endl;
    if (cc->type == CC_TYPE_BYTE || cc->type == CC_TYPE_WORD || cc->type == CC_TYPE_DWORD) {
      std::cout << "min: " << ctl_value(cc->type, cc->min) << std::endl;
      std::cout << "max: " << ctl_value(cc->type, cc->max) << std::endl;
      std::cout << "step: " << ctl_value(cc->type, cc->step) << std::endl;
    }
    std::cout << "}" << std::endl;
  }
}

int main(int, char const *[]) {
  err(c_init());
  open_cameras();
  configure_cameras();
  return 0;
}

