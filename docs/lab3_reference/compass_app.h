#ifndef __COMPASS_APP_H__
#define __COMPASS_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef COMPASS_SWAP_XY
#define COMPASS_SWAP_XY  0
#endif

#ifndef COMPASS_SIGN_X
#define COMPASS_SIGN_X   1
#endif

#ifndef COMPASS_SIGN_Y
#define COMPASS_SIGN_Y   1
#endif

#define COMPASS_READ_PERIOD_MS     100U
#define COMPASS_REPORT_PERIOD_MS   200U
#define COMPASS_RETRY_PERIOD_MS    1000U
#define COMPASS_SCALE_MIN          0
#define COMPASS_SCALE_MAX          200
#define COMPASS_SCALE_DEFAULT      100

void CompassApp_Init(void);
void CompassApp_Process(void);

#ifdef __cplusplus
}
#endif

#endif
