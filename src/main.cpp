#include <stdio.h>

#include <cmath>
#include <cstdio>
#include <variant>

#include "BNO08x/Adafruit_BNO08x.h"
#include "boards/pico.h"
#include "chassis.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "imu.h"
#include "path.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "quadrature_encoder.pio.h"

L298N driveRight(4, 5, 6);
L298N driveLeft(7, 8, 9);

const float FINISH_OFFSET = 16.f / 50;

PathVector points = PathVector{
    {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 2, 0}, {1, 2, 0}, {1, 3, 0}, {3, 3, 0}, {3, 2, 0}, {2 + FINISH_OFFSET, 2, 0}};


// 0 | 1 | 2 | 3
#define START_QUAD 0
#define TARGET_SECONDS 45

int main()
{
  // picotool configuration
  bi_decl(bi_program_description(
      "Science Olympiad Robot Tour - derock@derock.dev"));

  stdio_init_all();
  sleep_ms(1000);

  // initialize GPIO
  gpio_init(BEEPER_PIN);
  gpio_init(START_BUTTON_PIN);
  gpio_init(LIGHT_PIN);

  gpio_pull_up(START_BUTTON_PIN);
  gpio_set_dir(START_BUTTON_PIN, GPIO_IN);

  gpio_set_dir(BEEPER_PIN, GPIO_OUT);
  gpio_set_dir(LIGHT_PIN, GPIO_OUT);

  printf("[info] Starting...\n");
  for (int i = 1; i < 11; i++) // blink lights faster and faster
  {
    sleep_ms(100);
    printf("Starting...\n");
    gpio_put(LIGHT_PIN, 1);
    sleep_ms((11-i)*100);
    gpio_put(LIGHT_PIN, 0);
  }
  printf("[info] GPIO initialized\n");

  // initialize PIOs
  pio_add_program(pio0, &quadrature_encoder_program);
  quadrature_encoder_program_init(pio0, 0, LEFT_WHEEL_ENCODER, 0);

  pio_add_program(pio1, &quadrature_encoder_program);
  quadrature_encoder_program_init(pio1, 0, RIGHT_WHEEL_ENCODER, 0);

  printf("[info] PIO initialized\n");

  // beep once
  gpio_put(BEEPER_PIN, 1);
  sleep_ms(50);
  printf("[info] BEEEEEP!!!\n");
  gpio_put(BEEPER_PIN, 0);

  // setup BNO
  // hard reset
  printf("[info] BNO08x initializing...\n");
  while (!imu->begin_I2C(BNO08x_I2CADDR_DEFAULT, i2c0, 16, 17))
  {
    sleep_ms(100);
    printf("[info] BNO08x not found\n");
  };

  printf("[info] BNO08x found\n");
  imu->enableReport(SH2_ARVR_STABILIZED_RV, 5'000);
  printf("[info] BNO08x initialized\n");
  double heading = getHeading() * M_PI / 180.0f;
  printf("[debug] so now: %f\n", heading);

  // initialize odometry tracking and set initial position
  chassis::initializeOdometry();
  printf("[info] Odometry initialized\n");
  const Position START_POSITION = {50 * START_QUAD + 25, -14, 0};
  chassis::setPose(START_POSITION);
  printf("[info] Odometry initializing at (%f, %f, %f)\n", START_POSITION.x,
         START_POSITION.y, START_POSITION.theta);

  multicore_launch_core1(chassis::odometryTask);

  // beep once
  printf("[Info] BEEEEEP!!!\n");
  gpio_put(BEEPER_PIN, 1);
  sleep_ms(200);
  gpio_put(BEEPER_PIN, 0);

  //  Below are tests added by Dave

  printf("[Info] BLINK On Off!!!\n");
  gpio_put(LIGHT_PIN, 1);
  sleep_ms(1000);
  gpio_put(LIGHT_PIN, 0);
  printf("[Info] BLINK Off On!!!\n");
  sleep_ms(1000);

  // gen points

  // convert to centimeter
  printf("[info] running pathgen...\n");
  toAbsoluteCoordinates(points);

  printf("[debug] converted path:\n[debug]  <current %f, %f> ",
         chassis::getPosition().x, chassis::getPosition().y);
  for (Position segment : points)
  {
    printf("(%f, %f) -> ", segment.x, segment.y);
  }
  printf("\n");

  // interpolate all missing points
  std::vector<PathSegment> result;
  generatePath(points, result);
  printf("[info] pathgen done\n");

  // debug information
  printf("[debug] Planned path:\n");
  for (PathSegment segment : result)
  {
    if (std::holds_alternative<float>(segment.data))
    {
      printf("[debug] turn to %f\n", std::get<float>(segment.data));
    }
    else
    {
      PathVector path = std::get<PathVector>(segment.data);
      printf("[debug] follow path:\n");

      for (Position position : path)
      {
        printf("[debug]  - %f, %f\n", position.x, position.y);
      }
    }
  }

  // led and wait for start
  while (true)
  {
    gpio_put(LIGHT_PIN, 1);
    sleep_ms(50);
    gpio_put(LIGHT_PIN, 0);

    if (!gpio_get(START_BUTTON_PIN)) // pulled up
      break;
   

    sleep_ms(50);
  }
  gpio_put(LIGHT_PIN, 0);
  sleep_ms(100);

  // calculate end time
  int endTime = to_ms_since_boot(get_absolute_time()) + TARGET_SECONDS * 1000 +
                500; // over is better than under
  printf("[debug] current time is %d, target time is %d\n",
         to_ms_since_boot(get_absolute_time()), endTime);

  // run path
  // for (PathSegment segment : result) {
  for (int i = 0; i < result.size(); i++)
  {
    PathSegment segment = result.at(i);

    if (std::holds_alternative<float>(segment.data))
    {
      float targetHeading = std::get<float>(segment.data);
      printf("turning to %d\n", targetHeading);
      chassis::turnTo(targetHeading);
    }
    else
    {
      // calculate remaining distance
      float remainingDistance = 0;

      for (int y = i + 1; y < result.size(); y++)
      {
        PathSegment remainingPart = result.at(y);

        if (std::holds_alternative<PathVector>(remainingPart.data))
        {
          PathVector remainingPath = std::get<PathVector>(remainingPart.data);

          for (int z = 0; z < remainingPath.size() - 1; z++)
          {
            remainingDistance +=
                remainingPath[z].distance(remainingPath[z + 1]);
          }
        }
      }

      printf("Following path, remaining distance: %f\n", remainingDistance);
      chassis::follow(std::get<PathVector>(segment.data), 10, endTime,
                      remainingDistance);
    }
  }

  // main loop
  while (true)
  {
    gpio_put(LIGHT_PIN, 1);
    sleep_ms(1'000);
    gpio_put(LIGHT_PIN, 0);
    sleep_ms(1'000);
  }
}
