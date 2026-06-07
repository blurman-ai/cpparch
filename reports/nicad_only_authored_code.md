# only-NiCad authored clones — actual code

_All non-vendored clone groups NiCad found but archcheck did **not** report, with
the real source. Found by NiCad's `c` grammar on the files that parsed; missed by
archcheck for the reason noted per group. functions/blocks twins of one clone merged._

**Total distinct non-vendored only-NiCad clones: 19**


## LibreSprite (7 distinct)

### algo.cpp — size 3, sim 76, 54 lines
_why archcheck missed: implementation (just under archcheck similarity/joint-floor threshold)_

**[`src/doc/algo.cpp:464-534`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L464-L534)**
```cpp
{
  int npts;
  int out_x1, out_x2;
  int out_y1, out_y2;

  /* Derivatives of x(t) and y(t). */
  double x, dx, ddx, dddx;
  double y, dy, ddy, dddy;
  int i;

  /* Temp variables used in the setup. */
  double dt, dt2, dt3;
  double xdt2_term, xdt3_term;
  double ydt2_term, ydt3_term;

#define MAX_POINTS   64
#undef DIST
#define DIST(x, y) (sqrt((x) * (x) + (y) * (y)))
  npts = (int)(sqrt(DIST(x1-x0, y1-y0) +
                    DIST(x2-x1, y2-y1) +
                    DIST(x3-x2, y3-y2)) * 1.2);
  if (npts > MAX_POINTS)
    npts = MAX_POINTS;
  else if (npts < 4)
    npts = 4;

  dt = 1.0 / (npts-1);
  dt2 = (dt * dt);
  dt3 = (dt2 * dt);

  xdt2_term = 3 * (x2 - 2*x1 + x0);
  ydt2_term = 3 * (y2 - 2*y1 + y0);
  xdt3_term = x3 + 3 * (-x2 + x1) - x0;
  ydt3_term = y3 + 3 * (-y2 + y1) - y0;

  xdt2_term = dt2 * xdt2_term;
  ydt2_term = dt2 * ydt2_term;
  xdt3_term = dt3 * xdt3_term;
  ydt3_term = dt3 * ydt3_term;

  dddx = 6*xdt3_term;
  dddy = 6*ydt3_term;
  ddx = -6*xdt3_term + 2*xdt2_term;
  ddy = -6*ydt3_term + 2*ydt2_term;
  dx = xdt3_term - xdt2_term + 3 * dt * (x1 - x0);
  dy = ydt3_term - ydt2_term + dt * 3 * (y1 - y0);
  x = x0;
  y = y0;

  out_x1 = (int)x0;
  out_y1 = (int)y0;

  x += .5;
  y += .5;
  for (i=1; i<npts; i++) {
    ddx += dddx;
    ddy += dddy;
    dx += ddx;
    dy += ddy;
    x += dx;
    y += dy;

    out_x2 = (int)x;
    out_y2 = (int)y;

    proc(out_x1, out_y1, out_x2, out_y2, data);

    out_x1 = out_x2;
    out_y1 = out_y2;
  }
    /* ... truncated ... */
```
**[`src/doc/algo.cpp:539-612`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L539-L612)**
```cpp
{
  int npts;
  double out_x, old_x;
  double out_y, old_y;

  /* Derivatives of x(t) and y(t). */
  double x, dx, ddx, dddx;
  double y, dy, ddy, dddy;
  int i;

  /* Temp variables used in the setup. */
  double dt, dt2, dt3;
  double xdt2_term, xdt3_term;
  double ydt2_term, ydt3_term;

#define MAX_POINTS   64
#undef DIST
#define DIST(x, y) (sqrt ((x) * (x) + (y) * (y)))
  npts = (int) (sqrt (DIST(x1-x0, y1-y0) +
                      DIST(x2-x1, y2-y1) +
                      DIST(x3-x2, y3-y2)) * 1.2);
  if (npts > MAX_POINTS)
    npts = MAX_POINTS;
  else if (npts < 4)
    npts = 4;

  dt = 1.0 / (npts-1);
  dt2 = (dt * dt);
  dt3 = (dt2 * dt);

  xdt2_term = 3 * (x2 - 2*x1 + x0);
  ydt2_term = 3 * (y2 - 2*y1 + y0);
  xdt3_term = x3 + 3 * (-x2 + x1) - x0;
  ydt3_term = y3 + 3 * (-y2 + y1) - y0;

  xdt2_term = dt2 * xdt2_term;
  ydt2_term = dt2 * ydt2_term;
  xdt3_term = dt3 * xdt3_term;
  ydt3_term = dt3 * ydt3_term;

  dddx = 6*xdt3_term;
  dddy = 6*ydt3_term;
  ddx = -6*xdt3_term + 2*xdt2_term;
  ddy = -6*ydt3_term + 2*ydt2_term;
  dx = xdt3_term - xdt2_term + 3 * dt * (x1 - x0);
  dy = ydt3_term - ydt2_term + dt * 3 * (y1 - y0);
  x = x0;
  y = y0;

  out_x = old_x = x0;
  out_y = old_y = y0;

  x += .5;
  y += .5;
  for (i=1; i<npts; i++) {
    ddx += dddx;
    ddy += dddy;
    dx += ddx;
    dy += ddy;
    x += dx;
    y += dy;

    out_x = x;
    out_y = y;
    if (out_x > in_x) {
      out_y = old_y + (out_y-old_y) * (in_x-old_x) / (out_x-old_x);
      break;
    }
    old_x = out_x;
    old_y = out_y;
    /* ... truncated ... */
```
**[`src/doc/algo.cpp:617-690`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L617-L690)**
```cpp
{
  double out_x, old_x, old_dx, old_dy;
  int npts;

  /* Derivatives of x(t) and y(t). */
  double x, dx, ddx, dddx;
  double dy, ddy, dddy;
  int i;

  /* Temp variables used in the setup. */
  double dt, dt2, dt3;
  double xdt2_term, xdt3_term;
  double ydt2_term, ydt3_term;

#define MAX_POINTS   64
#undef DIST
#define DIST(x, y) (sqrt ((x) * (x) + (y) * (y)))
  npts = (int) (sqrt (DIST(x1-x0, y1-y0) +
                      DIST(x2-x1, y2-y1) +
                      DIST(x3-x2, y3-y2)) * 1.2);
  if (npts > MAX_POINTS)
    npts = MAX_POINTS;
  else if (npts < 4)
    npts = 4;

  dt = 1.0 / (npts-1);
  dt2 = (dt * dt);
  dt3 = (dt2 * dt);

  xdt2_term = 3 * (x2 - 2*x1 + x0);
  ydt2_term = 3 * (y2 - 2*y1 + y0);
  xdt3_term = x3 + 3 * (-x2 + x1) - x0;
  ydt3_term = y3 + 3 * (-y2 + y1) - y0;

  xdt2_term = dt2 * xdt2_term;
  ydt2_term = dt2 * ydt2_term;
  xdt3_term = dt3 * xdt3_term;
  ydt3_term = dt3 * ydt3_term;

  dddx = 6*xdt3_term;
  dddy = 6*ydt3_term;
  ddx = -6*xdt3_term + 2*xdt2_term;
  ddy = -6*ydt3_term + 2*ydt2_term;
  dx = xdt3_term - xdt2_term + 3 * dt * (x1 - x0);
  dy = ydt3_term - ydt2_term + dt * 3 * (y1 - y0);
  x = x0;

  out_x = x0;

  old_x = x0;
  old_dx = dx;
  old_dy = dy;

  x += .5;
  for (i=1; i<npts; i++) {
    ddx += dddx;
    ddy += dddy;
    dx += ddx;
    dy += ddy;
    x += dx;

    out_x = x;
    if (out_x > in_x) {
      dx = old_dx + (dx-old_dx) * (in_x-old_x) / (out_x-old_x);
      dy = old_dy + (dy-old_dy) * (in_x-old_x) / (out_x-old_x);
      break;
    }
    old_x = out_x;
    old_dx = dx;
    old_dy = dy;
    /* ... truncated ... */
```

### copy_cel.h — size 6, sim 70, 17 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/app/cmd/remove_palette.h:12-26`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/app/cmd/remove_palette.h#L12-L26)**
```cpp
namespace app {
namespace cmd {
  using namespace doc;

  class RemovePalette : public AddPalette {
  public:
    RemovePalette(Sprite* sprite, Palette& pal);

  protected:
    void onExecute() override;
    void onUndo() override;
  };

} // namespace cmd
} // namespace app
```
**[`src/app/cmd/remove_cel.h:12-27`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/app/cmd/remove_cel.h#L12-L27)**
```cpp
namespace app {
namespace cmd {
  using namespace doc;

  class RemoveCel : public AddCel {
  public:
    RemoveCel(std::shared_ptr<Cel> cel);

  protected:
    void onExecute() override;
    void onUndo() override;
    void onRedo() override;
  };

} // namespace cmd
} // namespace app
```
**[`src/app/cmd/remove_frame_tag.h:12-27`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/app/cmd/remove_frame_tag.h#L12-L27)**
```cpp
namespace app {
namespace cmd {
  using namespace doc;

  class RemoveFrameTag : public AddFrameTag {
  public:
    RemoveFrameTag(Sprite* sprite, FrameTag* tag);

  protected:
    void onExecute() override;
    void onUndo() override;
    void onRedo() override;
  };

} // namespace cmd
} // namespace app
```
_+3 more in this class: move_cel.h:19-40, copy_cel.h:19-40, remove_layer.h:12-27_

### algo.cpp — size 2, sim 93, 44 lines
_why archcheck missed: implementation (just under archcheck similarity/joint-floor threshold)_

**[`src/doc/algo.cpp:21-101`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L21-L101)**
```cpp
{
  int dx = x2-x1;
  int dy = y2-y1;
  int i1, i2;
  int x, y;
  int dd;

  /* worker macro */
#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)   \
  {                                                                     \
    if (d##pri_c == 0) {                                                \
      proc(x1, y1, data);                                               \
      return;                                                           \
    }                                                                   \
                                                                        \
    i1 = 2 * d##sec_c;                                                  \
    dd = i1 - (sec_sign (pri_sign d##pri_c));                           \
    i2 = dd - (sec_sign (pri_sign d##pri_c));                           \
                                                                        \
    x = x1;                                                             \
    y = y1;                                                             \
                                                                        \
    while (pri_c pri_cond pri_c##2) {                                   \
      proc(x, y, data);                                                 \
                                                                        \
      if (dd sec_cond 0) {                                              \
        sec_c sec_sign##= 1;                                            \
        dd += i2;                                                       \
      }                                                                 \
      else                                                              \
        dd += i1;                                                       \
                                                                        \
      pri_c pri_sign##= 1;                                              \
    }                                                                   \
  }

  if (dx >= 0) {
    if (dy >= 0) {
      if (dx >= dy) {
        /* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
        DO_LINE(+, x, <=, +, y, >=);
      }
      else {
        /* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
        DO_LINE(+, y, <=, +, x, >=);
      }
    }
    else {
      if (dx >= -dy) {
        /* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
        DO_LINE(+, x, <=, -, y, <=);
      }
      else {
        /* (x1 <= x2) && (y1 > y2) && (dx < dy) */
        DO_LINE(-, y, >=, +, x, >=);
      }
    }
  }
  else {
    if (dy >= 0) {
      if (-dx >= dy) {
        /* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
        DO_LINE(-, x, >=, +, y, >=);
      }
      else {
        /* (x1 > x2) && (y1 <= y2) && (dx < dy) */
        DO_LINE(+, y, <=, -, x, <=);
      }
    }
    else {
    /* ... truncated ... */
```
**[`src/doc/algo.cpp:106-190`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L106-L190)**
```cpp
{
  int dx = x2-x1;
  int dy = y2-y1;
  int i1, i2;
  int x, y;
  float f = 0;
  float fstep;
  int dd;


  /* worker macro */
#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)   \
  {                                                                     \
    if (d##pri_c == 0) {                                                \
      proc(x1, y1, f, data);                                            \
      return;                                                           \
    }                                                                   \
                                                                        \
    fstep = pri_sign 1.0f / d##pri_c;                                   \
    i1 = 2 * d##sec_c;                                                  \
    dd = i1 - (sec_sign (pri_sign d##pri_c));                           \
    i2 = dd - (sec_sign (pri_sign d##pri_c));                           \
                                                                        \
    x = x1;                                                             \
    y = y1;                                                             \
                                                                        \
    while (pri_c pri_cond pri_c##2) {                                   \
      proc(x, y, f += fstep, data);                                     \
                                                                        \
      if (dd sec_cond 0) {                                              \
        sec_c sec_sign##= 1;                                            \
        dd += i2;                                                       \
      }                                                                 \
      else                                                              \
        dd += i1;                                                       \
                                                                        \
      pri_c pri_sign##= 1;                                              \
    }                                                                   \
  }

  if (dx >= 0) {
    if (dy >= 0) {
      if (dx >= dy) {
        /* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
        DO_LINE(+, x, <=, +, y, >=);
      }
      else {
        /* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
        DO_LINE(+, y, <=, +, x, >=);
      }
    }
    else {
      if (dx >= -dy) {
        /* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
        DO_LINE(+, x, <=, -, y, <=);
      }
      else {
        /* (x1 <= x2) && (y1 > y2) && (dx < dy) */
        DO_LINE(-, y, >=, +, x, >=);
      }
    }
  }
  else {
    if (dy >= 0) {
      if (-dx >= dy) {
        /* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
        DO_LINE(-, x, >=, +, y, >=);
      }
      else {
        /* (x1 > x2) && (y1 <= y2) && (dx < dy) */
    /* ... truncated ... */
```

### algo.cpp — size 4, sim 71, 15 lines
_why archcheck missed: implementation (just under archcheck similarity/joint-floor threshold)_

**[`src/doc/algo.cpp:262-278`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L262-L278)**
```cpp
  for (;;) {
    err += xa + yy;
    if (err >= 0) {
      ya -= xx * 2;
      err -= ya;
      y--;
    }
    xa += yy * 2;
    x++;
    if (xa >= ya)
      break;

    proc(mx2 + x, my - y, data);
    proc(mx - x, my - y, data);
    proc(mx2 + x, my2 + y, data);
    proc(mx - x, my2 + y, data);
  }
```
**[`src/doc/algo.cpp:301-316`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L301-L316)**
```cpp
  for (;;) {
    err += ya + xx;
    if (err >= 0) {
      xa -= yy * 2;
      err -= xa;
      x--;
    }
    ya += xx * 2;
    y++;
    if (ya > xa)
      break;
    proc(mx2 + x, my - y, data);
    proc(mx - x, my - y, data);
    proc(mx2 + x, my2 + y, data);
    proc(mx - x, my2 + y, data);
  }
```
**[`src/doc/algo.cpp:388-406`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/doc/algo.cpp#L388-L406)**
```cpp
  for (;;) {
    err += xa + yy;
    if (err >= 0) {
      ya -= xx * 2;
      err -= ya;
      y--;
    }
    xa += yy * 2;
    x++;
    if (xa >= ya)
      break;

/*     proc(mx2 + x, my - y, data); */
/*     proc(mx - x, my - y, data); */
/*     proc(mx2 + x, my2 + y, data); */
/*     proc(mx - x, my2 + y, data); */
    proc(mx - x, my - y, mx2 + x, data);
    proc(mx - x, my2 + y, mx2 + x, data);
  }
```
_+1 more in this class: algo.cpp:429-446_

### chrono.h — size 4, sim 76, 14 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/she/osx/app.h:13-31`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/she/osx/app.h#L13-L31)**
```cpp
namespace she {

  class OSXApp {
  public:
    static OSXApp* instance();

    OSXApp();
    ~OSXApp();

    bool init();
    void activateApp();
    void finishLaunching();

  private:
    class Impl;
    Impl* m_impl;
  };

} // namespace she
```
**[`src/base/mutex.h:11-29`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/base/mutex.h#L11-L29)**
```cpp
namespace base {

  class mutex {
  public:
    mutex();
    ~mutex();

    void lock();
    bool try_lock();
    void unlock();

  private:
    class mutex_impl;
    mutex_impl* m_impl;

    DISABLE_COPYING(mutex);
  };

} // namespace base
```
**[`src/base/chrono.h:9-23`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/base/chrono.h#L9-L23)**
```cpp
namespace base {

  class Chrono {
  public:
    Chrono();
    ~Chrono();
    void reset();
    double elapsed() const;

  private:
    class ChronoImpl;
    ChronoImpl* m_impl;
  };

} // namespace base
```
_+1 more in this class: memory_dump.h:11-25_

### move_layer.h — size 2, sim 72, 25 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/app/cmd/trim_cel.h:16-37`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/app/cmd/trim_cel.h#L16-L37)**
```cpp
namespace app {
namespace cmd {

  class TrimCel : public Cmd {
  public:
    TrimCel(std::shared_ptr<doc::Cel> cel);
    ~TrimCel();

  protected:
    void onExecute() override;
    void onUndo() override;
    void onRedo() override;
    size_t onMemSize() const override {
      return sizeof(*this) + m_subCmd->memSize();
    }

  private:
    Cmd* m_subCmd;
  };

} // namespace cmd
} // namespace app
```
**[`src/app/cmd/move_layer.h:13-36`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/app/cmd/move_layer.h#L13-L36)**
```cpp
namespace app {
namespace cmd {
  using namespace doc;

  class MoveLayer : public Cmd {
  public:
    MoveLayer(Layer* layer, Layer* afterThis);

  protected:
    void onExecute() override;
    void onUndo() override;
    void onFireNotifications() override;
    size_t onMemSize() const override {
      return sizeof(*this);
    }

  private:
    WithLayer m_layer;
    WithLayer m_oldAfterThis;
    WithLayer m_newAfterThis;
  };

} // namespace cmd
} // namespace app
```

### autocrop.cpp — size 2, sim 100, 15 lines
_why archcheck missed: implementation (just under archcheck similarity/joint-floor threshold)_

**[`src/app/util/autocrop.cpp:23-64`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/app/util/autocrop.cpp#L23-L64)**
```cpp
{
#define SHRINK_SIDE(u_begin, u_op, u_final, u_add,              \
                    v_begin, v_op, v_final, v_add, U, V, var)   \
  do {                                                          \
    for (u = u_begin; u u_op u_final; u u_add) {                \
      for (v = v_begin; v v_op v_final; v v_add) {              \
        if (image->getPixel(U, V) != refpixel)                  \
          break;                                                \
      }                                                         \
      if (v == v_final)                                         \
        var;                                                    \
      else                                                      \
        break;                                                  \
    }                                                           \
  } while (0)

  int u, v;

  *x1 = 0;
  *y1 = 0;
  *x2 = image->width()-1;
  *y2 = image->height()-1;

  SHRINK_SIDE(0, <, image->width(), ++,
              0, <, image->height(), ++, u, v, (*x1)++);

  SHRINK_SIDE(0, <, image->height(), ++,
              0, <, image->width(), ++, v, u, (*y1)++);

  SHRINK_SIDE(image->width()-1, >, 0, --,
              0, <, image->height(), ++, u, v, (*x2)--);

  SHRINK_SIDE(image->height()-1, >, 0, --,
              0, <, image->width(), ++, v, u, (*y2)--);

  if ((*x1 > *x2) || (*y1 > *y2))
    return false;
  else
    return true;

#undef SHRINK_SIDE
}
```
**[`src/app/util/autocrop.cpp:68-109`](https://github.com/LibreSprite/LibreSprite/blob/276fdbdb27b537a074c3e170af6afc88c244a539/src/app/util/autocrop.cpp#L68-L109)**
```cpp
{
#define SHRINK_SIDE(u_begin, u_op, u_final, u_add,              \
                    v_begin, v_op, v_final, v_add, U, V, var)   \
  do {                                                          \
    for (u = u_begin; u u_op u_final; u u_add) {                \
      for (v = v_begin; v v_op v_final; v v_add) {              \
        if (image->getPixel(U, V) != refimage->getPixel(U, V))  \
          break;                                                \
      }                                                         \
      if (v == v_final)                                         \
        var;                                                    \
      else                                                      \
        break;                                                  \
    }                                                           \
  } while (0)

  int u, v;

  *x1 = 0;
  *y1 = 0;
  *x2 = image->width()-1;
  *y2 = image->height()-1;

  SHRINK_SIDE(0, <, image->width(), ++,
              0, <, image->height(), ++, u, v, (*x1)++);

  SHRINK_SIDE(0, <, image->height(), ++,
              0, <, image->width(), ++, v, u, (*y1)++);

  SHRINK_SIDE(image->width()-1, >, 0, --,
              0, <, image->height(), ++, u, v, (*x2)--);

  SHRINK_SIDE(image->height()-1, >, 0, --,
              0, <, image->width(), ++, v, u, (*y2)--);

  if ((*x1 > *x2) || (*y1 > *y2))
    return false;
  else
    return true;

#undef SHRINK_SIDE
}
```


## AetherSDR (11 distinct)

### ClientDeEssApplet.h — size 5, sim 70, 23 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/ClientRxDspApplet.h:5-29`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientRxDspApplet.h#L5-L29)**
```cpp
namespace AetherSDR {

class AudioEngine;
class AetherDspWidget;

// RX-side DSP applet — embeds AetherDspWidget as a docked tile inside the
// Aetherial Audio (PooDoo) container.  Same control set and persistence as
// the modeless AetherDspDialog (Settings menu); both views write to the
// same AppSettings keys, so changes in one update the other on next
// syncFromEngine().
class ClientRxDspApplet : public QWidget {
    Q_OBJECT

public:
    explicit ClientRxDspApplet(QWidget* parent = nullptr);

    void setAudioEngine(AudioEngine* engine);
    AetherDspWidget* widget() const { return m_widget; }

private:
    AudioEngine*     m_audio{nullptr};
    AetherDspWidget* m_widget{nullptr};
};

} // namespace AetherSDR
```
**[`src/gui/MeterApplet.h:5-40`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/MeterApplet.h#L5-L40)**
```cpp
namespace AetherSDR {

class MeterModel;
class HGauge;

// Radio hardware telemetry applet — shows PA temperature, supply voltage,
// and fan speed. Uses hwTelemetryChanged for cached meters (PATEMP, +13.8A)
// and meterUpdated for additional RAD meters resolved by index (MAINFAN).
//
// Note: PACURRENT is intentionally omitted — on FLEX-8000 series the meter
// range is capped at 10A (declared max) while real PA draw exceeds this at
// full power, causing the reading to clip. See FlexRadio community thread
// "PA Current Meter for 6xxx" and bug SMART-11281.
class MeterApplet : public QWidget {
    Q_OBJECT
public:
    explicit MeterApplet(QWidget* parent = nullptr);

    void setMeterModel(MeterModel* model);

private:
    void resolveIndices();
    void onMeterUpdated(int index, float value);

    MeterModel* m_model{nullptr};

    HGauge* m_paTempGauge{nullptr};
    HGauge* m_supplyGauge{nullptr};
    HGauge* m_fanGauge{nullptr};

    // Lazy-resolved meter index (-1 = not yet found)
    int m_fanIdx{-1};
    bool m_resolved{false};
};

} // namespace AetherSDR
```
**[`src/gui/ClientReverbApplet.h:7-38`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientReverbApplet.h#L7-L38)**
```cpp
namespace AetherSDR {

class AudioEngine;
class ClientCompKnob;

// Docked tile for the client-side TX reverb (Freeverb).  Compact
// 5-knob row — Size, Decay, Damping, PreDly, Mix — matches the PUDU
// applet footprint.  Bypass lives on the CHAIN widget single-click;
// the Aetherial Audio Channel Strip hosts the full editor.
class ClientReverbApplet : public QWidget {
    Q_OBJECT

public:
    explicit ClientReverbApplet(QWidget* parent = nullptr);

    void setAudioEngine(AudioEngine* engine);
    void refreshEnableFromEngine();

private:
    void buildUI();
    void syncKnobsFromEngine();

    AudioEngine*    m_audio{nullptr};
    ClientCompKnob* m_size{nullptr};
    ClientCompKnob* m_decay{nullptr};
    ClientCompKnob* m_damping{nullptr};
    ClientCompKnob* m_preDly{nullptr};
    ClientCompKnob* m_mix{nullptr};
    QWidget*        m_viz{nullptr};   // ReverbVizBox (defined in .cpp)
};

} // namespace AetherSDR
```
_+2 more in this class: PooDooLogo.h:7-43, ClientDeEssApplet.h:8-49_

### PgxlConnection.h — size 2, sim 89, 51 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/core/TgxlConnection.h:9-74`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/core/TgxlConnection.h#L9-L74)**
```cpp
namespace AetherSDR {

// Direct TCP connection to a 4O3A Tuner Genius XL on port 9010.
// Provides manual relay control (C1/L/C2) via the TGXL's native protocol,
// which is independent of the FlexRadio on port 4992.
//
// Protocol format (same style as SmartSDR):
//   C<seq>|<command>\n          — client command
//   R<seq>|<code>|<body>\n      — TGXL response
//   S0|state key=val ...\n      — unsolicited state push
//   V<version>\n                — version line on connect
//
// Reverse-engineered from 4O3A TGXL management app pcap (#469).
class TgxlConnection : public QObject {
    Q_OBJECT

public:
    explicit TgxlConnection(QObject* parent = nullptr);

    bool isConnected() const { return m_connected; }
    QString version() const { return m_version; }
    QString peerAddress() const { return m_socket.peerAddress().toString(); }
    quint16 peerPort() const { return m_socket.peerPort(); }

    void connectToTgxl(const QString& host, quint16 port = 9010);
    void disconnect();

    // Manual relay adjustment: relay 0=C1, 1=L, 2=C2; direction +1 or -1
    void adjustRelay(int relay, int direction);

    // Native autotune over the direct port-9010 channel. The TGXL drives
    // radio PTT via its hardware interlock cable, so no client-side keying
    // is required. Bypasses the firmware's `tgxl autotune` command path
    // (broken in firmware 4.2 — see issue tracker for "TUNE button on TGXL").
    void requestAutotune();

    // Send an arbitrary command to the TGXL (e.g. "activate ant=2")
    quint32 sendCommand(const QString& cmd);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    void stateUpdated(const QMap<QString, QString>& kvs);
    void statusUpdated(const QMap<QString, QString>& kvs);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void pollStatus();

private:
    void processLine(const QString& line);

    QTcpSocket m_socket;
    QTimer     m_pollTimer;       // 1/sec status poll
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    QString    m_version;
};

} // namespace AetherSDR
```
**[`src/core/PgxlConnection.h:9-55`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/core/PgxlConnection.h#L9-L55)**
```cpp
namespace AetherSDR {

// Direct TCP connection to a 4O3A Power Genius XL on port 9008.
// Same protocol as TgxlConnection (C/R/S/V message format).
// Provides PGXL status telemetry: state, power, SWR, current,
// temperature, mains voltage, band, bias mode, fan mode.
class PgxlConnection : public QObject {
    Q_OBJECT

public:
    explicit PgxlConnection(QObject* parent = nullptr);

    bool isConnected() const { return m_connected; }
    QString version() const { return m_version; }
    QString peerAddress() const { return m_socket.peerAddress().toString(); }
    quint16 peerPort() const { return m_socket.peerPort(); }

    void connectToPgxl(const QString& host, quint16 port = 9008);
    void disconnect();

    quint32 sendCommand(const QString& cmd);

signals:
    void connected();
    void disconnected();
    void statusUpdated(const QMap<QString, QString>& kvs);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void pollStatus();

private:
    void processLine(const QString& line);

    QTcpSocket m_socket;
    QTimer     m_pollTimer;
    QByteArray m_readBuf;
    quint32    m_seq{0};
    bool       m_connected{false};
    bool       m_gotVersion{false};
    QString    m_version;
};

} // namespace AetherSDR
```

### ClientCompThresholdFader.h — size 2, sim 93, 46 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/ClientEqOutputFader.h:7-73`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientEqOutputFader.h#L7-L73)**
```cpp
namespace AetherSDR {

// Combined vertical fader + level meter.  One custom-painted bar shows
// the post-EQ peak level as a gradient fill rising from the bottom, with
// a horizontal handle marker at the current output-gain position.  Drag
// the handle up/down to change gain, double-click to reset to 0 dB,
// scroll wheel for fine 0.5 dB steps.
//
// The widget writes a linear gain [0.0, ~4.0] back via gainChanged(); the
// editor's existing FFT timer feeds in a peak at ~25 Hz so the meter
// stays lively without a separate polling loop.
class ClientEqOutputFader : public QWidget {
    Q_OBJECT

public:
    explicit ClientEqOutputFader(QWidget* parent = nullptr);

    void setGainLinear(float linear);
    float gainLinear() const { return m_gain; }

    void setPeakLinear(float peakLinear);

signals:
    void gainChanged(float linear);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;

private:
    void refreshValueLabel();
    void setGainFromY(int y);

    QLabel* m_valueLabel{nullptr};
    float   m_gain{1.0f};
    float   m_smoothedPeak{-120.0f};  // dB
    bool    m_dragging{false};

    // dB ranges
    static constexpr float kGainMinDb  = -36.0f;
    static constexpr float kGainMaxDb  = +12.0f;
    static constexpr float kMeterMinDb = -60.0f;
    static constexpr float kMeterMaxDb =   0.0f;

    // Layout constants
    static constexpr int kLabelColW = 20;
    static constexpr int kGap       = 2;
    static constexpr int kBarW      = 16;
    static constexpr int kHandleOverhang = 4;   // handle sticks out on each side
    static constexpr int kHandleH        = 3;

    // Vertical padding inside the fader strip so the handle can reach the
    // top / bottom ends without clipping.
    static constexpr int kStripTopPad    = 4;
    static constexpr int kStripBottomPad = 4;

    // Cached strip rect — recomputed in paintEvent.  Used by mouse handlers
    // so they don't recompute geometry on every move.
    int m_stripTop{0};
    int m_stripH{0};
};

} // namespace AetherSDR
```
**[`src/gui/ClientCompThresholdFader.h:7-71`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientCompThresholdFader.h#L7-L71)**
```cpp
namespace AetherSDR {

// Combined input-level meter + threshold fader for the compressor
// editor.  Visual + interaction pattern mirrors ClientEqOutputFader —
// one custom-painted strip with a peak-level gradient fill plus a
// horizontal handle overhanging both sides of the bar.  Dragging the
// handle emits thresholdChanged(db); that same value is what the
// threshold chevron on the curve canvas represents, so the two
// controls stay in lockstep via the shared ClientComp state.
//
// Level scale is absolute dBFS [-60, 0], matching ClientCompMeter's
// Level mode.  Handle range is the same: you can pull the threshold
// anywhere the input meter can display.
class ClientCompThresholdFader : public QWidget {
    Q_OBJECT

public:
    explicit ClientCompThresholdFader(QWidget* parent = nullptr);

    void setThresholdDb(float db);
    float thresholdDb() const { return m_thresholdDb; }

    // Feed the latest input peak (dBFS).  Peak-follower smoothing is
    // applied internally so the bar doesn't flicker on silent frames.
    void setInputPeakDb(float db);

signals:
    void thresholdChanged(float db);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;

private:
    void refreshValueLabel();
    void setThresholdFromY(int y);

    QLabel* m_valueLabel{nullptr};
    float   m_thresholdDb{-18.0f};
    float   m_smoothedPeakDb{-120.0f};
    bool    m_dragging{false};

    static constexpr float kMeterMinDb = -60.0f;
    static constexpr float kMeterMaxDb =   0.0f;
    static constexpr float kThreshMinDb = -60.0f;
    static constexpr float kThreshMaxDb =   0.0f;
    static constexpr float kThreshDefaultDb = -18.0f;

    static constexpr int kLabelColW      = 22;
    static constexpr int kGap            = 2;
    static constexpr int kBarW           = 16;
    static constexpr int kHandleOverhang = 4;
    static constexpr int kHandleH        = 3;
    static constexpr int kStripTopPad    = 4;
    static constexpr int kStripBottomPad = 4;

    int m_stripTop{0};
    int m_stripH{0};
};

} // namespace AetherSDR
```

### ClientDeEssCurveWidget.h — size 3, sim 74, 29 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/PanFloatingWindow.h:7-44`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/PanFloatingWindow.h#L7-L44)**
```cpp
namespace AetherSDR {

class PanadapterApplet;

// Top-level window hosting a detached panadapter. Created by
// PanadapterStack::floatPanadapter(), destroyed on dock.
class PanFloatingWindow : public QWidget {
    Q_OBJECT

public:
    explicit PanFloatingWindow(QWidget* parent = nullptr);

    void adoptApplet(PanadapterApplet* applet);
    PanadapterApplet* takeApplet();
    PanadapterApplet* applet() const { return m_applet; }
    QString panId() const;

    void saveWindowGeometry();
    void restoreWindowGeometry();
    void setShuttingDown(bool on) { m_shuttingDown = on; }

    // Apply or remove Qt::FramelessWindowHint at runtime to match the
    // main-window frameless setting.  Preserves geometry and visibility.
    void setFramelessMode(bool on);

signals:
    void dockRequested(const QString& panId);

protected:
    void closeEvent(QCloseEvent* ev) override;

private:
    PanadapterApplet* m_applet{nullptr};
    QVBoxLayout* m_layout{nullptr};
    bool m_shuttingDown{false};
};

} // namespace AetherSDR
```
**[`src/gui/ClientTubeCurveWidget.h:7-49`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientTubeCurveWidget.h#L7-L49)**
```cpp
namespace AetherSDR {

class ClientTube;

// Transfer-curve display for the tube saturator.  X axis is input
// amplitude in [-1.5, +1.5] (so the curve shows how the shaper
// behaves above full scale as well as below); Y axis is output
// amplitude over the same range.  The curve bends according to the
// current Model / Drive / Bias values.  A live ball tracks the
// instantaneous input amplitude along the curve so the user can see
// which part of the transfer function the signal is riding on.
class ClientTubeCurveWidget : public QWidget {
    Q_OBJECT

public:
    explicit ClientTubeCurveWidget(QWidget* parent = nullptr);

    void setTube(ClientTube* t);
    ClientTube* tube() const { return m_tube; }

    void setCompactMode(bool on);

    static constexpr float kAxisLimit = 1.5f;   // display range on both axes

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    float xToPx(float x) const;
    float yToPx(float y) const;
    // Evaluate the current model's transfer function at input x.
    // Mirrors ClientTube::shape() math but takes only the user's
    // current Drive + Bias into account (no envelope modulation —
    // the curve shows the static response).
    float evalCurve(float x) const;

    ClientTube* m_tube{nullptr};
    QTimer*     m_pollTimer{nullptr};
    bool        m_compact{false};
    float       m_lastInputLin{0.0f};   // smoothed ball position
};

} // namespace AetherSDR
```
**[`src/gui/ClientDeEssCurveWidget.h:9-62`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientDeEssCurveWidget.h#L9-L62)**
```cpp
namespace AetherSDR {

class ClientDeEss;

// Compact sidechain response display for the docked de-esser tile.
// Draws the biquad bandpass magnitude curve across 100 Hz .. 12 kHz
// (log-frequency), with a vertical marker at the threshold level on
// a dB scale, and a live dot tracking the sidechain envelope so the
// user can see when the de-esser is triggering.
//
// Thread model: ClientDeEss parameter + meter reads are already
// atomic internally, so paintEvent + polling QTimer are UI-thread
// safe without extra locking.
class ClientDeEssCurveWidget : public QWidget {
    Q_OBJECT

public:
    explicit ClientDeEssCurveWidget(QWidget* parent = nullptr);

    void setDeEss(ClientDeEss* d);
    ClientDeEss* deEss() const { return m_deEss; }

    void setCompactMode(bool on);

    // Frequency range drawn on the X axis (log scale).
    static constexpr float kMinHz = 100.0f;
    static constexpr float kMaxHz = 12000.0f;
    // dB range on the Y axis for the magnitude plot.
    static constexpr float kMinDb = -40.0f;
    static constexpr float kMaxDb =  12.0f;

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    float hzToX(float hz) const;
    float dbToY(float db) const;
    // RBJ bandpass magnitude at `hz` using the current filter params.
    float bandpassMagDb(float hz) const;

    ClientDeEss* m_deEss{nullptr};
    QTimer*      m_pollTimer{nullptr};
    bool         m_compact{false};
    float        m_lastScDb{-120.0f};   // smoothed sidechain ball dB

    // Cached axis labels — one QStaticText per kFreqMajor entry.  Font
    // is fixed at 8 px regardless of compact mode (compact gates label-
    // draw entirely), so the cache is built once and never invalidated
    // beyond the initial dirty flag.
    mutable QVector<QStaticText> m_axisLabels;
    mutable bool                 m_labelsDirty{true};
};

} // namespace AetherSDR
```

### DaxApplet.h — size 2, sim 75, 27 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/TciApplet.h:9-61`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/TciApplet.h#L9-L61)**
```cpp
namespace AetherSDR {

class RadioModel;
class TciServer;
class MeterSlider;

// TCI Applet — TCI WebSocket server control panel.
// Mirrors DaxApplet framework: per-channel meters + gain sliders for 4 RX
// channels plus 1 TX channel, a port QLineEdit, and an Enable toggle.
//
// Entire implementation is gated by HAVE_WEBSOCKETS. When the build does not
// include WebSocket support, the applet compiles to an empty widget.
class TciApplet : public QWidget {
    Q_OBJECT

public:
    static constexpr int kChannels = 4;

    explicit TciApplet(QWidget* parent = nullptr);

    void setRadioModel(RadioModel* model);
    void setTciServer(TciServer* tci);

    // Sync Enable button state (called by MainWindow on autostart)
    void setTciEnabled(bool on);
    void setTciRxLevel(int channel, float rms);  // channel 1-4
    void setTciTxLevel(float rms);

signals:
    void tciToggled(bool on);
    void tciRxGainChanged(int channel, float gain);  // 1-4, 0.0–1.0
    void tciTxGainChanged(float gain);

private:
#ifdef HAVE_WEBSOCKETS
    void buildUI();
    void updateTciStatus();

    RadioModel* m_model{nullptr};
    TciServer*  m_tciServer{nullptr};

    QPushButton* m_tciEnable{nullptr};
    QLineEdit*   m_tciPort{nullptr};
    QLabel*      m_tciStatus{nullptr};

    MeterSlider* m_rxMeter[kChannels]{};
    QLabel*      m_rxStatus[kChannels]{};
    MeterSlider* m_txMeter{nullptr};
    QLabel*      m_txStatus{nullptr};
#endif
};

} // namespace AetherSDR
```
**[`src/gui/DaxApplet.h:8-48`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/DaxApplet.h#L8-L48)**
```cpp
namespace AetherSDR {

class RadioModel;
class SliceModel;
class MeterSlider;

// DAX Applet — DAX Audio channel meters + gain sliders.
// Displays RX meters for DAX channels 1-4 plus a single TX meter.
class DaxApplet : public QWidget {
    Q_OBJECT

public:
    static constexpr int kChannels = 4;

    explicit DaxApplet(QWidget* parent = nullptr);

    void setRadioModel(RadioModel* model);

    // Sync Enable button state (called by MainWindow on autostart)
    void setDaxEnabled(bool on);
    void setDaxRxLevel(int channel, float rms);  // channel 1-4
    void setDaxTxLevel(float rms);

signals:
    void daxToggled(bool on);
    void daxRxGainChanged(int channel, float gain);  // 1-4, 0.0–1.0
    void daxTxGainChanged(float gain);

private:
    void buildUI();

    RadioModel* m_model{nullptr};

    QPushButton*  m_daxEnable{nullptr};
    MeterSlider*  m_daxRxMeter[kChannels]{};
    QLabel*       m_daxRxStatus[kChannels]{};
    MeterSlider*  m_daxTxMeter{nullptr};
    QLabel*       m_daxTxStatus{nullptr};
};

} // namespace AetherSDR
```

### ClientEqIconRow.h — size 2, sim 87, 25 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/ClientEqIconRow.h:8-51`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientEqIconRow.h#L8-L51)**
```cpp
namespace AetherSDR {

class AudioEngine;

// Top-of-editor row: one small icon button per active band showing the
// filter-type curve shape (HP slope, low shelf, bell, high shelf, LP slope)
// in the band's palette color. Click cycles type forward; shift-click
// cycles backward. Right-click exposes the explicit type menu (handled
// by the canvas's existing context menu — icons route through the same
// AudioEngine mutation path).
//
// The row reflows when activeBandCount() changes; selected band gets a
// bright ring. Inactive trailing slots are hidden (not shown greyed).
class ClientEqIconRow : public QWidget {
    Q_OBJECT

public:
    explicit ClientEqIconRow(QWidget* parent = nullptr);

    void setEq(ClientEq* eq);
    void setAudioEngine(AudioEngine* engine);  // for persistence on edit

signals:
    void bandSelected(int idx);

public slots:
    // Called when the ClientEq instance changes on the audio side (add /
    // delete band, type change via context menu, path switch). Rebuilds
    // the icon row to match the current state.
    void refresh();
    void setSelectedBand(int idx);

private:
    class IconButton;  // defined in the .cpp

    void rebuild();

    ClientEq*    m_eq{nullptr};
    AudioEngine* m_audio{nullptr};
    QHBoxLayout* m_layout{nullptr};
    int          m_selectedBand{-1};
};

} // namespace AetherSDR
```
**[`src/gui/ClientEqParamRow.h:8-43`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientEqParamRow.h#L8-L43)**
```cpp
namespace AetherSDR {

// Bottom-of-editor strip: one column per active band, stacking frequency
// (Hz), gain (dB), and Q as text labels in the band's palette colour.
// Selected band gets a boxed outline around the gain value — the
// Logic-Pro-style "this is what you're tweaking" affordance.
//
// Clicking a column selects that band. No inline editing yet — a future
// PR can promote the labels to click-to-edit.
class ClientEqParamRow : public QWidget {
    Q_OBJECT

public:
    explicit ClientEqParamRow(QWidget* parent = nullptr);

    void setEq(ClientEq* eq);

signals:
    void bandSelected(int idx);

public slots:
    void refresh();           // rebuild columns to match current band count
    void refreshValues();     // update text without re-laying-out (drag path)
    void setSelectedBand(int idx);

private:
    class Column;  // implemented in the .cpp

    void rebuild();

    ClientEq*    m_eq{nullptr};
    QHBoxLayout* m_layout{nullptr};
    int          m_selectedBand{-1};
};

} // namespace AetherSDR
```

### CompactColorPicker.h — size 2, sim 89, 20 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/CompactColorPicker.h:44-62`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/CompactColorPicker.h#L44-L62)**
```cpp
class HueStrip : public QWidget {
    Q_OBJECT
public:
    explicit HueStrip(QWidget* parent = nullptr);
    void setHue(int h);    // 0–359
    int  hue() const { return m_h; }

signals:
    void huePicked(int h);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    void pickFromMouse(const QPoint& p);
    int m_h{180};
};
```
**[`src/gui/CompactColorPicker.h:64-84`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/CompactColorPicker.h#L64-L84)**
```cpp
class AlphaSlider : public QWidget {
    Q_OBJECT
public:
    explicit AlphaSlider(QWidget* parent = nullptr);
    void setAlpha(int a);                 // 0–255
    void setBaseColor(const QColor& c);   // R/G/B at alpha 255
    int  alpha() const { return m_a; }

signals:
    void alphaPicked(int a);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    void pickFromMouse(const QPoint& p);
    int    m_a{255};
    QColor m_base{0, 180, 216};
};
```

### EditorFramelessTitleBar.h — size 2, sim 70, 20 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/FramelessWindowTitleBar.h:7-27`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/FramelessWindowTitleBar.h#L7-L27)**
```cpp
namespace AetherSDR {

class FramelessWindowTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit FramelessWindowTitleBar(const QString& title, QWidget* parent = nullptr);

    void setTitleText(const QString& title);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QLabel* m_titleLabel{nullptr};
};

} // namespace AetherSDR
```
**[`src/gui/EditorFramelessTitleBar.h:7-49`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/EditorFramelessTitleBar.h#L7-L49)**
```cpp
namespace AetherSDR {

// Reusable 20 px-tall title bar for the PooDoo Audio editor windows
// (parametric EQ, compressor, gate, tube, PUDU, reverb, de-esser).
// Each editor sets Qt::FramelessWindowHint at construction and adds
// this widget at the very top of its layout.  Behaviour:
//
//  - Press-and-drag anywhere on the bar moves the window.  macOS uses
//    a manual move path because repeated QWindow::startSystemMove()
//    calls can be refused for several seconds after a move completes.
//  - Double-click toggles maximize.
//  - The trio at the right (— □ ✕) wires to showMinimized /
//    showMaximized / close on the host window via an installed event
//    filter on each glyph QLabel.
//  - setTitleText() drives the heading on the left so the host editor
//    can flip the label when its Side / Path changes.
class EditorFramelessTitleBar : public QWidget {
public:
    explicit EditorFramelessTitleBar(QWidget* parent = nullptr);

    void setTitleText(const QString& text);

    // Hide the min / max / close trio while leaving the title label
    // visible.  Used by the AetherialAudioStrip when it embeds the
    // per-stage editors — the strip owns its own window controls, so
    // each embedded panel just needs the name plate (#2301).
    void setControlsVisible(bool on);

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    QLabel* m_titleLbl{nullptr};
    QLabel* m_minLbl{nullptr};
    QLabel* m_maxLbl{nullptr};
    QLabel* m_closeLbl{nullptr};
};

} // namespace AetherSDR
```

### MultiFlexDialog.h — size 2, sim 70, 19 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/WaveformsDialog.h:8-33`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/WaveformsDialog.h#L8-L33)**
```cpp
namespace AetherSDR {

class FlexWaveformModel;

// Non-modal dialog for WFP status and waveform management (File → Waveforms).
// Mirrors the SmartSDR File → Waveforms panel: shows WFP power/ready/IP at the
// top and one row per installed waveform with Restart and Remove/Uninstall
// buttons.  Connects directly to FlexWaveformModel signals so it stays live
// without any MainWindow involvement in refresh.
class WaveformsDialog : public PersistentDialog {
    Q_OBJECT

public:
    explicit WaveformsDialog(FlexWaveformModel* model, QWidget* parent = nullptr);

private:
    void refreshStatus();
    void refreshWaveformList();

    FlexWaveformModel* m_model;
    QLabel*            m_statusLabel{nullptr};
    QWidget*           m_listContainer{nullptr};
    QVBoxLayout*       m_listLayout{nullptr};
};

} // namespace AetherSDR
```
**[`src/gui/MultiFlexDialog.h:9-29`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/MultiFlexDialog.h#L9-L29)**
```cpp
namespace AetherSDR {

class RadioModel;

class MultiFlexDialog : public PersistentDialog {
    Q_OBJECT
public:
    explicit MultiFlexDialog(RadioModel* model, QWidget* parent = nullptr);

private:
    void refresh();

    RadioModel* m_model;
    QTableWidget* m_table;
    QPushButton* m_enableBtn;
    QLabel* m_pttLabel;
    QPushButton* m_pttBtn;
    bool m_refreshing{false};
};

} // namespace AetherSDR
```

### ClientCompLimiterButton.h — size 2, sim 73, 17 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/PhaseKnob.h:5-29`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/PhaseKnob.h#L5-L29)**
```cpp
namespace AetherSDR {

// Polar display for ESC (Enhanced Signal Clarity).
// Shows a crosshair circle with a dot positioned by phase (angle) and
// gain (distance from center). Phase and gain are set externally via
// sliders — this widget is display-only.
class PhaseKnob : public QWidget {
    Q_OBJECT

public:
    explicit PhaseKnob(QWidget* parent = nullptr);

    // Phase in radians (0 – 2π), gain 0.0 – 2.0
    void setPhase(float radians);
    void setGain(float gain);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    float m_phase{0.0f};   // radians
    float m_gain{1.0f};    // 0.0 – 2.0 (1.0 = half radius)
};

} // namespace AetherSDR
```
**[`src/gui/ClientCompLimiterButton.h:6-41`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/ClientCompLimiterButton.h#L6-L41)**
```cpp
namespace AetherSDR {

// LIMIT toggle with a built-in activity indicator.  When the user
// enables the limiter (checkable), the button reads as "armed".
// Whenever ClientComp::limiterActive() returns true, the caller
// strobes the button via setActive(true) — the button then glows
// red for ~120 ms before decaying back to "armed" so brief firings
// still register visually.
//
// Kept as a subclass so the paint logic stays self-contained and
// the editor can treat it as a drop-in for QPushButton.
class ClientCompLimiterButton : public QPushButton {
    Q_OBJECT

public:
    explicit ClientCompLimiterButton(QWidget* parent = nullptr);

    // Called from the editor's meter-poll tick with the latest
    // limiterActive state.  Transitions from false→true restart the
    // hold timer; the button redraws accordingly.
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    bool m_active{false};
    QElapsedTimer m_activeHold;
    // 500 ms hold after the last active tick — long enough that a brief
    // limiter firing still leaves a visible red flash, short enough that
    // sustained program material doesn't leave the button constantly
    // red after the signal goes quiet.
    static constexpr int kHoldMs = 500;
};

} // namespace AetherSDR
```

### DxClusterStartupCommandsDialog.h — size 2, sim 75, 14 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/gui/DxClusterStartupCommandsDialog.h:7-38`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/DxClusterStartupCommandsDialog.h#L7-L38)**
```cpp
namespace AetherSDR {

// Modal editor for the per-instance startup-commands list (#2683).  One
// command per line; the backend (DxClusterClient::sendStartupCommands)
// replays the list after every login, including reconnects.
//
// Use the static edit() helper at the call site — it constructs the
// dialog with the right AppSettings key and runs the modal exec()
// loop.  Two keys exist in practice: "DxClusterStartupCommands" for
// the main cluster tab and "RbnStartupCommands" for the Reverse Beacon
// Network tab, kept independent so operators can configure each
// cluster service separately.
class DxClusterStartupCommandsDialog : public PersistentDialog {
    Q_OBJECT

public:
    explicit DxClusterStartupCommandsDialog(const QString& title,
                                            const QString& appSettingsKey,
                                            QWidget* parent = nullptr);

    // Convenience launcher.  Opens the editor populated from the given
    // AppSettings key and writes back on OK (no-op on Cancel).
    static void edit(const QString& title,
                     const QString& appSettingsKey,
                     QWidget* parent = nullptr);

private:
    QPlainTextEdit* m_edit{nullptr};
    QString m_key;
};

} // namespace AetherSDR
```
**[`src/gui/SwrSweepLicenseDialog.h:7-36`](https://github.com/aethersdr/AetherSDR/blob/44ce91c3cf42e4a6aa7c5f552dc662c7e7c753aa/src/gui/SwrSweepLicenseDialog.h#L7-L36)**
```cpp
namespace AetherSDR {

// Modal license-confirmation dialog gating the Antenna SWR sweep.
//
// First time the user hits Start Sweep, this dialog appears with an
// operator-responsibility disclaimer (mirrors the ATU Band Pre-Tune
// disclaimer) and two buttons: "I am licensed to use this feature"
// and "Cancel".  A "Remember my answer" checkbox persists the
// confirmation to AppSettings under SwrSweepLicenseConfirmed so
// subsequent presses go straight to the sweep without the popup.
//
// Use the static confirm() helper at the call site — it handles the
// AppSettings short-circuit, the modal exec(), and the persistence
// write so callers don't need to manage any of that themselves.
class SwrSweepLicenseDialog : public PersistentDialog {
    Q_OBJECT

public:
    explicit SwrSweepLicenseDialog(QWidget* parent = nullptr);

    // True when the user has either previously confirmed (with the
    // remember-my-answer checkbox) or just confirmed in this session.
    // Returns false if the user cancels.  Safe to call repeatedly.
    static bool confirm(QWidget* parent = nullptr);

private:
    QCheckBox* m_rememberCheck{nullptr};
};

} // namespace AetherSDR
```


## Kartend (1 distinct)

### animation.h — size 23, sim 72, 17 lines
_why archcheck missed: HEADER declaration (archcheck scans implementation, not class decls)_

**[`src/ui/uiconstants/detailspaneconstants.h:4-53`](https://github.com/EtherAura/Kartend/blob/54bfbdfc29c0387b466ac43fcff28e960a35dbd0/src/ui/uiconstants/detailspaneconstants.h#L4-L53)**
```cpp
namespace UIConstants {

// =============================================================================
// DetailsPane (introduced the rename; promoted the
// namespace to UIConstants::DetailsPane.)
// Details-pane dimensions and timing.
// =============================================================================
namespace DetailsPane {
/// Minimum details-pane width in pixels (Left/Right dock).
inline constexpr int MIN_WIDTH = 150;
/// Default details-pane width when first shown (Left/Right dock).
inline constexpr int FIXED_WIDTH = 300;
/// minimum details-pane height in pixels (Top/Bottom dock).
inline constexpr int MIN_HEIGHT = 120;
/// default details-pane height in pixels (Top/Bottom dock).
inline constexpr int FIXED_HEIGHT = 280;
/// Margin around details-pane content.
inline constexpr int MARGIN = 0;
/// Offset to account for scrollbar width.
inline constexpr int SCROLLBAR_OFFSET = 80;
/// Additional margin offset for layout.
inline constexpr int MARGIN_OFFSET = 40;
/// Delay before recalculating metrics after resize.
inline constexpr int METRICS_RECALC_DELAY_MS = 200;
/// Delay before notifying layout change.
inline constexpr int LAYOUT_NOTIFY_DELAY_MS = 100;
/// Delay before initial center scroll on show.
inline constexpr int INITIAL_CENTER_SCROLL_DELAY_MS = 50;
/// Delay after selection settles before starting preview video playback.
/// Was 500ms pre-Kartend-9q8d when the debounce had to be long enough to
/// outwait scroll animations (otherwise playVideo would block the GUI
/// thread mid-glide and stutter the scroll). Now Kartend-9q8d round 6's
/// scroll-idle predicate guards playVideo independently, so this debounce
/// only needs to confirm the user actually settled on an item — 200ms is
/// snappier without re-introducing the mid-scroll-block risk.
inline constexpr int VIDEO_PREVIEW_DEBOUNCE_MS = 200;
/// Debounce window for the sidebar metadata refresh (4 DB queries +
/// filesystem probes). Coalesces wheel/arrow storms (~10ms cadence) into a
/// single refresh once the selection settles. Tuned short enough that a
/// single click feels instant but long enough that a 30-tick wheel sweep
/// fires once instead of 30 times.
inline constexpr int METADATA_DEBOUNCE_MS = 60;
/// width of the resize grip on the inner edge (px). Has to be
/// wide enough to be a forgiving target but narrow enough that it doesn't
/// eat clicks aimed at controls near the inner edge (gallery edit button,
/// metadata buttons). 8 strikes the balance — also paired with an event
/// filter on inner widgets so children don't swallow grip-zone clicks.
inline constexpr int RESIZE_GRIP_PX = 8;
} // namespace DetailsPane
} // namespace UIConstants
```
**[`src/ui/uiconstants/selection.h:4-36`](https://github.com/EtherAura/Kartend/blob/54bfbdfc29c0387b466ac43fcff28e960a35dbd0/src/ui/uiconstants/selection.h#L4-L36)**
```cpp
namespace UIConstants {

// =============================================================================
// Selection
// Timing for selection restore process and double-click handling.
// =============================================================================
namespace Selection {
/// Number of steps in progressive selection restore
inline constexpr int RESTORE_STEPS = 6;
/// Delay between each restore step
inline constexpr int RESTORE_STEP_DELAY_MS = 120;
/// Maximum total time for selection restore process
inline constexpr int RESTORE_MAX_DELAY_MS = 900;
/// First early verification check during restore
inline constexpr int RESTORE_EARLY_VERIFY_1_MS = 140;
/// Second early verification check during restore
inline constexpr int RESTORE_EARLY_VERIFY_2_MS = 320;
/// Suppress double-click immediately after subcollection enter
inline constexpr int DOUBLE_CLICK_SUPPRESS_AFTER_ENTER_MS = 700;
/// Delay before clearing double-click suppression
inline constexpr int DOUBLE_CLICK_SUPPRESS_CLEAR_DELAY_MS = 800;
/// Debounce delay before pushing the selected item's metadata into the
/// sidebar. Prevents thrash during rapid keyboard / mouse navigation.
inline constexpr int METADATA_SIDEBAR_UPDATE_DELAY_MS = 120;
/// Selection-overlay glide animation: travel speed in pixels-per-second.
/// Used to derive a duration proportional to the distance the overlay must
/// move so short hops feel snappy and long jumps stay readable.
inline constexpr double OVERLAY_GLIDE_PIXELS_PER_SECOND = 1500.0;
/// Hard cap on selection-overlay glide animation duration (ms).
/// Prevents very long jumps from feeling sluggish.
inline constexpr int OVERLAY_GLIDE_MAX_DURATION_MS = 300;
} // namespace Selection
} // namespace UIConstants
```
**[`src/ui/uiconstants/keyboard.h:4-30`](https://github.com/EtherAura/Kartend/blob/54bfbdfc29c0387b466ac43fcff28e960a35dbd0/src/ui/uiconstants/keyboard.h#L4-L30)**
```cpp
namespace UIConstants {

// =============================================================================
// Keyboard Navigation
// Timing for arrow key navigation, repeat rates, and centering behavior.
// =============================================================================
namespace Keyboard {
/// Base interval between arrow key repeat steps
inline constexpr int BASE_INTERVAL_MS = 260;
/// Extra delay added for vertical navigation (row changes)
inline constexpr int VERTICAL_EXTRA_MS = 120;
/// Time to wait for animation to complete before next step
inline constexpr int ANIMATION_SETTLE_MS = 200;
/// Initial delay before key repeat begins
inline constexpr int REPEAT_START_DELAY_MS = 260;
/// Timer interval for view updates during navigation
inline constexpr int VIEW_UPDATE_INTERVAL_MS = 30;
/// Delay to clear arrow center suppression after selection restore
inline constexpr int ARROW_CENTER_CLEAR_AFTER_RESTORE_MS = 300;
/// Extended suppression after selection restore completes
inline constexpr int ARROW_CENTER_EXTRA_SUPPRESS_AFTER_RESTORE_MS = 500;
/// Delay before checking if arrow center can be cleared
inline constexpr int ARROW_CENTER_CLEAR_CHECK_DELAY_MS = 440;
/// Delay to clear arrow center suppression after explicit set
inline constexpr int ARROW_CENTER_CLEAR_AFTER_SET_MS = 240;
} // namespace Keyboard
} // namespace UIConstants
```
_+20 more in this class: grid.h:4-37, timing.h:4-48, mouse.h:4-40, item.h:4-36, metadata.h:4-45, dialog.h:4-45, cache.h:4-44, database.h:4-81, detailspane.h:4-53, artwork.h:4-51, dialog.h:23-44, listview.h:4-29, animation.h:4-32, attract.h:4-36, overlay.h:4-24, navigation.h:4-25, widget.h:4-52, placeholder.h:4-52, attract.h:10-35, launch.h:4-27_
