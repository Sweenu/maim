#include "x.hpp"

glm::vec4 getWindowGeometry( X11* x11, Window win ) {
    XWindowAttributes attr;         
    XGetWindowAttributes( x11->display, win, &attr );
    unsigned int width = attr.width;           
    unsigned int height = attr.height;         
    unsigned int border = attr.border_width;   
    int x, y;
    Window junk;
    XTranslateCoordinates( x11->display, win, attr.root, -attr.border_width, -attr.border_width, &x, &y, &junk );
    width += border*2;
    height += border*2;
    return glm::vec4( x, y, width, height );
}

X11::X11( std::string displayName ) {
	// Initialize display
	display = XOpenDisplay( displayName.c_str() );
	if ( !display ) {
		throw new std::runtime_error(std::string("Error: Failed to open X display: ") + displayName );
	}
	screen = ScreenOfDisplay( display, DefaultScreen( display ) );
	visual = DefaultVisual( display, XScreenNumberOfScreen( screen ) );
	root = DefaultRootWindow( display );
    int major = 0;
    int minor = 0;
    Bool pixmaps = true;
    haveXShm = XShmQueryVersion( display, &major, &minor, &pixmaps );
    haveXShm = (haveXShm && pixmaps );
}

X11::~X11() {
	XCloseDisplay( display );
}

XImage* X11::getImage( Window draw, int x, int y, int w, int h, glm::ivec2& imageloc ) {
    // We need to clamp the selection to fit within the
    // provided window.
    glm::ivec4 sourceGeo = getWindowGeometry( this, draw );
    x = glm::clamp( x, 0, sourceGeo.z-1 );
    y = glm::clamp( y, 0, sourceGeo.w-1 );
    w = glm::clamp( w, 1, sourceGeo.z-x );
    h = glm::clamp( h, 1, sourceGeo.w-y );

    imageloc = glm::ivec2(x,y);

    if ( haveXShm ) {
        return getImageShm( draw, x, y, w, h );
    }
    return XGetImage( display, draw, x, y, w, h, AllPlanes, ZPixmap );
}

// Basically a faster image grabber.
XImage* X11::getImageShm(Window draw, int x, int y, int w, int h) {
	XImage* xim;
	XShmSegmentInfo thing;

    XWindowAttributes xattr;
    Status s = XGetWindowAttributes (display, draw, &xattr);

	/* try create an shm image */
	xim = XShmCreateImage(display, xattr.visual, xattr.depth, ZPixmap, 0, &thing, w, h);
	if (!xim) {
		return None;
	}

	/* setup a temporary error handler */
	//XErrorHandler ph = XSetErrorHandler(TmpXError);

	/* get an shm id of this image */
	thing.shmid = shmget(IPC_PRIVATE, xim->bytes_per_line * xim->height, IPC_CREAT | 0666);
	/* if the get succeeds */
	if (thing.shmid != -1) {
		/* set the params for the shm segment */
		thing.readOnly = False;
		thing.shmaddr = xim->data = (char*)shmat(thing.shmid, 0, 0);
		/* get the shm addr for this data chunk */
		if (xim->data != (char *)-1) {
			XShmAttach(display, &thing);
            XShmGetImage(display, draw, xim, x, y, 0xffffffff);
            return xim;
			//shmdt(thing.shmaddr);
		}
		/* get failed - out of shm id's or shm segment too big ? */
		/* remove the shm id we created */
		shmctl(thing.shmid, IPC_RMID, 0);
	}

	/* couldnt create SHM image ? */
	/* destroy previous image */
	//XDestroyImage(xim);

	return None;
}
