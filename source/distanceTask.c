#include "distanceTask.h"
#include "common.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_debug_console.h"


/* Helper function to measure the echo pulse (Requires Hardware Timer) */
static uint32_t MeasureEchoTimeUs(void) {
    uint32_t timeUs = 0;

    // 1. Wait for ECHO pin to go HIGH
    // 2. Start Hardware Timer
    // 3. Wait for ECHO pin to go LOW
    // 4. Stop Hardware Timer & read elapsed time

    return timeUs;
}


void vDistanceTask(void *pvParameters) {
    SensorData_t distanceData;
    distanceData.sensorType = SENSOR_TYPE_DISTANCE;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1 Hz polling

    for(;;) {
        /* 1. Activate the ultrasonic sensor */
        GPIO_PinWrite(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN, 1);
        GPIO_PinWrite(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN, 0);

        /* 2. Measure Echo & Calculate */
        uint32_t echoTimeUs = MeasureEchoTimeUs();

        // Distance = (Time in us * 0.0343) / 2
        float distanceCm = (echoTimeUs * 0.0343f) / 2.0f;

        /* 3. Send to Queue */
        distanceData.value = distanceCm;

        if (xQueueSend(xSensorQueue, &distanceData, pdMS_TO_TICKS(10)) != pdPASS) {
            PRINTF("DistanceTask: Queue Full!\r\n");
        }

        /* 4. Sleep */
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
