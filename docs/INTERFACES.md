# Interfaces implemented (derived from the ROM's own services.jar / oplus-framework.jar)

## vendor.oplus.hardware.touch.IOplusTouch  (AIDL, VERSION 2, hash ef41c5fab372bb5ad6a417f606845c87cf0e9b17)
instance: `vendor.oplus.hardware.touch.IOplusTouch/default`
| code | method |
|---|---|
| 1 | int initialize() |
| 2 | int isTouchNodeSupport(int node, int dev) |
| 3 | String touchReadNodeFile(int node, int dev) |
| 4 | int touchWriteNodeFile(int node, int dev, String val) |
| 5 | int touchWriteBtInfo(int node, int dev, String val) |
| 6 | oneway void touchWriteNodeFileOneWay(int node, int dev, String val) |
| 7 | int touchNotifyClient(int cmd, OplusTouchInfo info)  // parcelable {long,int,int,int,String} |
| 8 | int registerEventCallback(IOplusTouchEventCallback cb) |
| 9 | int unregisterEventCallback(IOplusTouchEventCallback cb) |

ColorOS node ids (OplusTouchNodeManager): DOUBLE_TAP_ENABLE=1, COORDINATE=2, BASELINE_TEST=3,
CALIBRATION=4, DIRECTION=5, LIMIT_WHITELIST=6, LIMIT_ENABLE=7, KERNEL_GRIP=8, REPORT_RATE_WL=9,
CHARGE_DETECT=10, WIRELESS_CHARGE=11, HEADSET=12, BLACK_SCREEN_TEST=13 ... DOUBLE_TAP_INDEP=21.

## vendor.oplus.hardware.displaypanelfeature.IDisplayPanelFeature  (AIDL, VERSION 1, hash 39afca22ac77253421ad681eeb2bae0c6ff62c13)
| code | method |
|---|---|
| 1 | int getDisplayPanelFeatureValue(int feature, out int[] value) |
| 2 | int setDisplayPanelFeatureValue(int feature, in int[] value) |
| 3 | int getDisplayPanelInfo(int id, out List<String> info) |

Feature ids used by ColorOS: AOD=7, MAX_BRIGHTNESS=8, PANEL_INFO=9, HBM=12, DIM_ALPHA=13,
PANEL_BLANK=24, FPPRESS=28, NORMAL_MAXBRIGHTNESS=247, NOTIFY_DISP_FINGER_LAYER=20004
(finger layer show/hide = 1/0, MTK variant inverted 0/1).

## Xiaomi side (from the vendor's own AIDL libs, for later phases)
ITouchFeature: setModeValue(int,int,int,int*), getModeCurValue, registerCallback, ...
IDisplayFeature: setFeature(int,int,int,int), sendMessage(int,int,string), notifyBrightness(int)
IXiaomiFingerprint: extCmd(int,int,int*)
