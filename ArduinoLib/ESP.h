#ifndef _ESP_H
#define _ESP_H

#include "Arduino.h"

class ESP {

    public:

        ESP();

	void wdtDisable(void)
	{
            // noop
	}

	void wdtFeed(void)
	{
            // noop
	}

	uint32_t getFreeHeap(void) 
        {
            return (0);
        }

        int checkFlashCRC()
        {
            return (1);
        }

        void restart (bool minus_K, bool minus_0);

        uint32_t getChipId(void);

    private:

        void addArgv (char **&argv, int &argc, const char *arg);
        uint32_t sn;
};

extern class ESP ESP;

extern void yield(void);

#endif // _ESP_H
