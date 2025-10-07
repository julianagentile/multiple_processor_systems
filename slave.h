#ifndef __SLAVE_PROCESS_H__
#define __SLAVE_PROCESS_H__

#include "RayTrace.h"

void slaveMain( ConfigData *data );


// void staticStripsHorizontalSlave(ConfigData* data );
void staticStripsVerticalSlave(ConfigData* data );
void staticSquareBlocksSlave(ConfigData* data );
void dynamicSlave(ConfigData* data );

#endif
