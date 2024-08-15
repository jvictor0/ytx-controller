#ifndef PERIODIC_TASK_H
#define PERIODIC_TASK_H

#include <Arduino.h>

class PeriodicTask {
	public:
		PeriodicTask(int sampleRate);

		void begin();
		
		void start();
		void reset();
		void stop();
	private:
		bool IsSyncing();
};

#endif //PERIODIC_TASK_H