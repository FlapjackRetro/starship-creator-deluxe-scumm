import sys, time, ctypes
x11 = ctypes.CDLL("libX11.so.6"); xt = ctypes.CDLL("libXtst.so.6")
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XOpenDisplay.argtypes=[ctypes.c_char_p]
x11.XKeysymToKeycode.argtypes=[ctypes.c_void_p, ctypes.c_ulong]
x11.XStringToKeysym.restype = ctypes.c_ulong
x11.XStringToKeysym.argtypes=[ctypes.c_char_p]
x11.XFlush.argtypes=[ctypes.c_void_p]
xt.XTestFakeMotionEvent.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_ulong]
xt.XTestFakeButtonEvent.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_ulong]
xt.XTestFakeKeyEvent.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_int,ctypes.c_ulong]
D = x11.XOpenDisplay(b":77")
def move(x,y): xt.XTestFakeMotionEvent(D,0,x,y,0); x11.XFlush(D)
def click(x,y,b=1):
    move(x,y); time.sleep(.2)
    xt.XTestFakeButtonEvent(D,b,1,0); x11.XFlush(D); time.sleep(.1)
    xt.XTestFakeButtonEvent(D,b,0,0); x11.XFlush(D)
def key(name):
    kc = x11.XKeysymToKeycode(D, x11.XStringToKeysym(name.encode()))
    xt.XTestFakeKeyEvent(D,kc,1,0); x11.XFlush(D); time.sleep(.05)
    xt.XTestFakeKeyEvent(D,kc,0,0); x11.XFlush(D)
def typestr(s):
    names={' ':'space','\n':'Return','_':'underscore','-':'minus','.':'period','/':'slash'}
    for ch in s:
        key(names.get(ch, ch)); time.sleep(.08)

if __name__=="__main__":
    a=sys.argv[1:]
    if a[0]=="click": click(int(a[1]),int(a[2]))
    elif a[0]=="key": key(a[1])
    elif a[0]=="type": typestr(a[1].replace("\\n","\n"))
    time.sleep(.1)
