#include "nvs_storage.h"
#include "logger.h"

CalibrationRecord savedCal = {};
bool baselineValid = false;

uint32_t checksumCal(const CalibrationRecord &r) {
  const uint8_t *bytes = (const uint8_t*)&r;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < offsetof(CalibrationRecord, checksum); i++) {
    h ^= bytes[i];
    h *= 16777619u;
  }
  return h;
}

void loadCalibration() {
  baselineValid = false;
  Preferences prefs;
  if (!prefs.begin("fordwin", false)) {
    logEvent("[NVS] Cannot open preferences.");
    return;
  }

  CalibrationRecord rec = {};
  size_t n = 0;
  if (prefs.getBytesLength("cal_v1") == sizeof(rec)) {
    n = prefs.getBytes("cal_v1", &rec, sizeof(rec));
  }
  prefs.end();

  if (n == sizeof(rec) && rec.magic == CAL_MAGIC && rec.version == CAL_VERSION &&
      rec.checksum == checksumCal(rec) && rec.baselineMv >= 100 && rec.baselineMv <= 3100 &&
      rec.samples >= CAL_MIN_SAMPLES) {
    savedCal = rec;
    baselineValid = true;
    logEvent(String("[NVS] Loaded BASE=") + String(savedCal.baselineMv) + "mV | TEST LIMIT=" +
             String(savedCal.baselineMv + ANTI_PINCH_MARGIN_MV) + "mV");
  } else {
    logEvent("[NVS] No baseline. AUTO UP disabled until CAL success.");
  }
}

bool saveCalibration(int baseline, int idle, int p10, int p90, int count) {
  CalibrationRecord r = {};
  r.magic = CAL_MAGIC;
  r.version = CAL_VERSION;
  r.idleMv = idle;
  r.baselineMv = baseline;
  r.marginMvAtSave = ANTI_PINCH_MARGIN_MV;
  r.p10Mv = p10;
  r.p90Mv = p90;
  r.samples = count;
  r.checksum = checksumCal(r);

  Preferences prefs;
  if (!prefs.begin("fordwin", false)) return false;
  size_t n = prefs.putBytes("cal_v1", &r, sizeof(r));
  prefs.end();

  if (n != sizeof(r)) return false;
  savedCal = r;
  baselineValid = true;
  return true;
}
