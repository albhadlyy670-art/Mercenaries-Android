#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct AInputEvent AInputEvent;
enum{AINPUT_EVENT_TYPE_KEY=1,AINPUT_EVENT_TYPE_MOTION=2,AINPUT_SOURCE_JOYSTICK=0x1000010,AINPUT_SOURCE_TOUCHSCREEN=0x1002,
AMOTION_EVENT_ACTION_MASK=0xff,AMOTION_EVENT_ACTION_POINTER_INDEX_MASK=0xff00,AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT=8,
AMOTION_EVENT_ACTION_DOWN=0,AMOTION_EVENT_ACTION_UP=1,AMOTION_EVENT_ACTION_MOVE=2,AMOTION_EVENT_ACTION_CANCEL=3,AMOTION_EVENT_ACTION_POINTER_DOWN=5,AMOTION_EVENT_ACTION_POINTER_UP=6,
AKEY_EVENT_ACTION_DOWN=0,AMOTION_EVENT_AXIS_X=0,AMOTION_EVENT_AXIS_Y=1,AMOTION_EVENT_AXIS_Z=11,AMOTION_EVENT_AXIS_RZ=14,AMOTION_EVENT_AXIS_LTRIGGER=17,AMOTION_EVENT_AXIS_RTRIGGER=18,AMOTION_EVENT_AXIS_BRAKE=23,AMOTION_EVENT_AXIS_GAS=22,AMOTION_EVENT_AXIS_HAT_X=15,AMOTION_EVENT_AXIS_HAT_Y=16};
int32_t AInputEvent_getType(const AInputEvent*);int32_t AInputEvent_getSource(const AInputEvent*);
int32_t AMotionEvent_getAction(const AInputEvent*);size_t AMotionEvent_getPointerCount(const AInputEvent*);int32_t AMotionEvent_getPointerId(const AInputEvent*,size_t);
float AMotionEvent_getX(const AInputEvent*,size_t);float AMotionEvent_getY(const AInputEvent*,size_t);float AMotionEvent_getAxisValue(const AInputEvent*,int32_t,size_t);
int32_t AKeyEvent_getAction(const AInputEvent*);int32_t AKeyEvent_getKeyCode(const AInputEvent*);
