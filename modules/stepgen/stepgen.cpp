
#include "stepgen.h"
#include "../remora.h"
//#include "../boardconfig.h"


// If undef then we use bresenham
#undef USE_FLOAT_DDS


/***********************************************************************
                MODULE CONFIGURATION AND CREATION FROM JSON     
************************************************************************/

void createStepgen()
{
    const char* comment = module["Comment"];
    printf("\n%s\n",comment);

    int joint = module["Joint Number"];
    const char* step = module["Step Pin"];
    const char* dir = module["Direction Pin"];

    // configure pointers to data source and feedback location
    //ptrJointFreqCmd[joint] = &rxData.jointFreqCmd[joint];
    //ptrJointFeedback[joint] = &txData.jointFeedback[joint];
    //ptrJointEnable = &rxData.jointEnable;

    // create the step generator, register it in the thread
    Module* stepgen = new Stepgen(base_freq, joint, step, dir, STEPBIT);
    baseThread->registerModule(stepgen);
    baseThread->registerModulePost(stepgen);
}


/***********************************************************************
    MODULE CONFIGURATION AND CREATION FROM STATIC CONFIG - boardconfi.h   
************************************************************************/

void loadStaticStepgen()
{

}

/***********************************************************************
                METHOD DEFINITIONS
************************************************************************/

Stepgen::Stepgen(int32_t threadFreq, int jointNumber, std::string step, std::string direction, int stepBit) :
	jointNumber(jointNumber),
	step(step),
	direction(direction),
	stepBit(stepBit)
{
	this->stepPin = new Pin(this->step, OUTPUT);
	this->directionPin = new Pin(this->direction, OUTPUT);
	this->DDSaccumulator = 0;
	this->rawCount = 0;
	this->frequencyScale = (float)(1 << this->stepBit) / (float)threadFreq;
	this->mask = 1 << this->jointNumber;
	this->isEnabled = false;
	this->isForward = false;
	this->threadFreq = threadFreq;
	this->bresenhamD = 0;
}


void Stepgen::update()
{
	// Use the standard Module interface to run makePulses()
	this->makePulses();
}

void Stepgen::updatePost()
{
	this->stopPulses();
}

void Stepgen::slowUpdate()
{
	return;
}

void Stepgen::makePulses()
{
	int32_t stepNow = 0;

	this->rxData = getCurrentRxBuffer(&rxPingPongBuffer);
	this->txData = getCurrentTxBuffer(&txPingPongBuffer);

	this->isEnabled = ((rxData->jointEnable & this->mask) != 0);

#ifdef USE_FLOAT_DDS
	if (this->isEnabled == true)  												// this Step generator is enables so make the pulses
	{
		this->frequencyCommand = rxData->jointFreqCmd[this->jointNumber];             // Get the latest frequency command via pointer to the data source
		this->DDSaddValue = this->frequencyCommand * this->frequencyScale;		// Scale the frequency command to get the DDS add value
		stepNow = this->DDSaccumulator;                           				// Save the current DDS accumulator value
		this->DDSaccumulator += this->DDSaddValue;           	  				// Update the DDS accumulator with the new add value
		stepNow ^= this->DDSaccumulator;                          				// Test for changes in the low half of the DDS accumulator
		stepNow &= (1L << this->stepBit);                         				// Check for the step bit
		//this->rawCount = this->DDSaccumulator >> this->stepBit;   				// Update the position raw count

		if (this->DDSaddValue > 0)												// The sign of the DDS add value indicates the desired direction
		{
			this->isForward = true;
		}
		else
		{
			this->isForward = false;
		}

		if (stepNow)
		{
			this->directionPin->set(this->isForward);             		    	// Set direction pin
			this->stepPin->set(true);											// Raise step pin
			if (this->isForward)
            {
                ++this->rawCount;
            }
            else
            {
                --this->rawCount;
            }
            txData->jointFeedback[this->jointNumber] = this->rawCount;	
		}
	}
#else
	if(this->isEnabled) {
		int32_t freq = rxData->jointFreqCmd[this->jointNumber];
		if(freq < 0){
			isForward = false;
			freq = -freq;
		} else {
			isForward = true;
		}
		int32_t dy = freq;
		int32_t dx = threadFreq;
		if(bresenhamD > 0) {
			// Update bresenham state.
			bresenhamD -= 2 * dx;
			// Set pins
			directionPin->set(isForward);
			stepPin->set(true);
			// Update count
			rawCount += isForward ? 1 : -1;
			txData->jointFeedback[this->jointNumber] = this->rawCount;	
		}
		bresenhamD += 2 * dy;

	}
#endif



}


void Stepgen::stopPulses()
{
	this->stepPin->set(false);	// Reset step pin
}


void Stepgen::setEnabled(bool state)
{
	this->isEnabled = state;
}
