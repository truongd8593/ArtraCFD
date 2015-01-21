/****************************************************************************
 * Numeric Scheme in Time Doamin                                            *
 * Last-modified: 21 Jan 2015 11:05:57 AM
 * Programmer: Huangrui Mo                                                  *
 * - Follow the Google's C/C++ style Guide.                                 *
 * - This file defines the numeric schemes of time domain.                  *
 ****************************************************************************/
/****************************************************************************
 * Required Header Files
 ****************************************************************************/
#include "rungekutta.h"
#include <stdio.h> /* standard library for input and output */
#include <stdlib.h> /* dynamic memory allocation and exit */
#include <math.h> /* common mathematical functions */
#include <string.h> /* manipulating strings */
#include "gcibm.h"
#include "ensight.h"
#include "commons.h"
/****************************************************************************
 * Function definitions
 ****************************************************************************/
int RungeKuttaTimeMarching(Field *field, Flux *flux, Space *space, 
        Particle *particle, Time *time, const Partition *part, const Flow *flow)
{
    ShowInformation("Time marching...");
    double exportTimeInterval = (time->totalTime - time->currentTime) / time->totalOutputTimes;
    double accumulatedTime = 0;
    /* time marching */
    for (time->stepCount += 1; time->currentTime < time->totalTime; ++time->stepCount) {
        /*
         * Calculate dt for current time step
         */
        time->dt = exportTimeInterval;
        /*
         * Update current time stamp, if current time exceeds the total time, 
         * recompute the value of dt to make current time equal total time.
         */
        time->currentTime = time->currentTime + time->dt;
        if (time->currentTime > time->totalTime) { /* need to refine "dt" to reach totTime  */
            time->dt = time->totalTime - (time->currentTime - time->dt);
            time->currentTime = time->totalTime;
        }
        /*
         * Compute field data in current time step
         */
        printf("\nStep=%d; Time=%lg; dt=%lg\n", time->stepCount, time->currentTime, time->dt);
        /*
         * Export computed data. Use accumulatedTime as a flag, if
         * accumulatedTime increases to anticipated export interval,
         * write data out. Because the accumulatedTime very likely
         * can not increase to the exporting interval at the last
         * phase, then add a extra condition that if current time
         * is the total time, then also write data out.
         */
        accumulatedTime = accumulatedTime + time->dt;
        if ((accumulatedTime >=  exportTimeInterval) || 
                (fabs(time->currentTime - time->totalTime) < 1e-100)) {
            ++time->outputCount; /* export count increase */
            WriteComputedDataEnsight(field->Uo, space, particle, time, part);
            accumulatedTime = 0; /* reset accumulated time */
        }
        /* fluid solid coupling */
        /* particle dynamics */
        /* recompute domain geometry and remeshing */
    }
    ShowInformation("Session End");
    return 0;
}
/* a good practice: end file with a newline */

