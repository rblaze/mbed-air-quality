#include <BME280.h>
#include <CCS811.h>
#include <ST7036i.h>
#include <TextLCD.h>
#include <mbed.h>
#include <mbed_events.h>

I2C bus(MBED_CONF_APP_BUS_SDA, MBED_CONF_APP_BUS_SCL);
BME280 bme280{bus, 0x77 << 1};
CCS811 ccs811{bus, 0x5b << 1};
TextLCD<text_lcd::ST7036i_20x2> lcd{bus, 0x78};
DigitalOut led{LED1, 0};

char bme_data[lcd.width()] = "starting up, wait";
char ccs_data[lcd.width()] = "micro meteo station";

void updateBmeData();
void readBmeData();
void printData();

void readCcsData();

EventQueue* queue{mbed_event_queue()};
auto updateBmeDataEvent{queue->make_user_allocated_event(updateBmeData)};
auto readBmeDataEvent{queue->make_user_allocated_event(readBmeData)};
auto printDataEvent{queue->make_user_allocated_event(printData)};

auto readCcsDataEvent{queue->make_user_allocated_event(readCcsData)};

void updateBmeData() {
  bme280.setForcedMode();
  readBmeDataEvent.call();
}

void readBmeData() {
  bme280.updateData();
  snprintf(bme_data, lcd.width(), "%ldC | %lu%% | %lukPa        ",
           bme280.getTemperature() / 100, bme280.getHumidity() / 1024,
           bme280.getPressure() / 1000);
}

void printData() {
  lcd.moveCursorHome();
  lcd.write(bme_data, lcd.width());
  lcd.moveCursorTo(0, 1);
  lcd.write(ccs_data, lcd.width());
  led = !led;
  printf("%20s\n", bme_data);
  printf("%20s\n\n", ccs_data);
}

void readCcsData() {
  // Convert BME280 data to CSS811
  uint32_t humidity = bme280.getHumidity() / 2;
  int32_t bme_temp = bme280.getTemperature();
  uint32_t temp;
  if (bme_temp < -2500) {
    temp = 0;
  } else {
    temp = bme_temp + 2500;  // Add 25C shift
    temp *= 128;             // Multiply by 512 and divide by 100
    temp /= 25;
  }

  ccs811.setEnvData(humidity, temp);

  const char* msg;
  switch (ccs811.refreshData()) {
    case CCS811::CCS811_OK:
      msg = "";
      break;
    case CCS811::CCS811_STALE_DATA:
      msg = "stale";
      break;
    default:
      msg = "error";
      break;
  }

  snprintf(ccs_data, lcd.width(), "CO2 %u TV %u %s         ", ccs811.getCO2(),
           ccs811.getTVOC(), msg);
}

int main() {
  bus.frequency(100000);
  lcd.init();
  lcd.displayControl(true, false, false);

  bme280.init();
  bme280.setConfig(BME280::Config::WEATHER_MONITORING);

  ccs811.init();
  ccs811.setMode(CCS811::Mode::EVERY_60_S);

  // UserAllocatedEvent doesn't have "duration" overrides for delay() and
  // period()
  updateBmeDataEvent.period(5000);  // 5s
  readBmeDataEvent.delay(bme280.getUpdateDelay().count());

  readCcsDataEvent.delay(10000); // Wait for BME to get first reading
  readCcsDataEvent.period(60000);

  printDataEvent.period(1000);

  updateBmeDataEvent.call();
  readCcsDataEvent.call();
  printDataEvent.call();

  queue->dispatch_forever();
}
