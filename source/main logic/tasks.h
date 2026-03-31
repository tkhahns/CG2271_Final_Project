#ifndef TASKS_H
#define TASKS_H

// FreeRTOS task entry points — pass to xTaskCreate
void sensorTask(void *p);
void buttonTask(void *p);
void alertTask(void *p);
void printTask(void *p);

#endif
