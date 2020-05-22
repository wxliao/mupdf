/* Device interface */

typedef struct NativeDeviceInfo NativeDeviceInfo;

typedef int (NativeDeviceLockFn)(JNIEnv *env, NativeDeviceInfo *info);
typedef void (NativeDeviceUnlockFn)(JNIEnv *env, NativeDeviceInfo *info);

struct NativeDeviceInfo
{
	/* Some devices (like the AndroidDrawDevice, or DrawDevice) need
	 * to lock/unlock the java object around device calls. We have functions
	 * here to do that. Other devices (like the DisplayList device) need
	 * no such locking, so these are NULL. */
	NativeDeviceLockFn *lock; /* Function to lock */
	NativeDeviceUnlockFn *unlock; /* Function to unlock */
	jobject object; /* The java object that needs to be locked. */

	/* Conceptually, we support drawing onto a 'plane' of pixels.
	 * The plane is width/height in size. The page is positioned on this
	 * at xOffset,yOffset. We want to redraw the given patch of this.
	 *
	 * The samples pointer in pixmap is updated on every lock/unlock, to
	 * cope with the object moving in memory.
	 */
	fz_pixmap *pixmap;
	int xOffset;
	int yOffset;
	int width;
	int height;
};

static NativeDeviceInfo *lockNativeDevice(JNIEnv *env, jobject self, int *err)
{
	NativeDeviceInfo *info = NULL;

	*err = 0;
	if (!(*env)->IsInstanceOf(env, self, cls_NativeDevice))
		return NULL;

	info = CAST(NativeDeviceInfo *, (*env)->GetLongField(env, self, fid_NativeDevice_nativeInfo));
	if (!info)
	{
		/* Some devices (like the Displaylist device) need no locking, so have no info. */
		return NULL;
	}
	info->object = (*env)->GetObjectField(env, self, fid_NativeDevice_nativeResource);

	if (info->lock(env, info))
	{
		*err = 1;
		return NULL;
	}

	return info;
}

static void unlockNativeDevice(JNIEnv *env, NativeDeviceInfo *info)
{
	if (info)
		info->unlock(env, info);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_finalize)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	NativeDeviceInfo *ninfo;

	if (!ctx) return;

	FUN(Device_finalize)(env, self); /* Call super.finalize() */

	ninfo = CAST(NativeDeviceInfo *, (*env)->GetLongField(env, self, fid_NativeDevice_nativeInfo));
	if (ninfo)
	{
		fz_drop_pixmap(ctx, ninfo->pixmap);
		fz_free(ctx, ninfo);
	}
}

JNIEXPORT void JNICALL
FUN(NativeDevice_close)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_close_device(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}
JNIEXPORT void JNICALL
FUN(NativeDevice_fillPath)(JNIEnv *env, jobject self, jobject jpath, jboolean even_odd, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	int err;

	if (!ctx || !dev) return;
	if (!path) return jni_throw_arg(env, "path must not be null");
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_path(ctx, dev, path, even_odd, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_strokePath)(JNIEnv *env, jobject self, jobject jpath, jobject jstroke, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!path) return jni_throw_arg(env, "path must not be null");
	if (!stroke) return jni_throw_arg(env, "stroke must not be null");
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_stroke_path(ctx, dev, path, stroke, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipPath)(JNIEnv *env, jobject self, jobject jpath, jboolean even_odd, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!path) return jni_throw_arg(env, "path must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_path(ctx, dev, path, even_odd, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipStrokePath)(JNIEnv *env, jobject self, jobject jpath, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_path *path = from_Path(env, jpath);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!path) return jni_throw_arg(env, "path must not be null");
	if (!stroke) return jni_throw_arg(env, "stroke must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_stroke_path(ctx, dev, path, stroke, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillText)(JNIEnv *env, jobject self, jobject jtext, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) return jni_throw_arg(env, "text must not be null");
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_text(ctx, dev, text, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_strokeText)(JNIEnv *env, jobject self, jobject jtext, jobject jstroke, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) return jni_throw_arg(env, "text must not be null");
	if (!stroke) return jni_throw_arg(env, "stroke must not be null");
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_stroke_text(ctx, dev, text, stroke, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipText)(JNIEnv *env, jobject self, jobject jtext, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) return jni_throw_arg(env, "text must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_text(ctx, dev, text, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipStrokeText)(JNIEnv *env, jobject self, jobject jtext, jobject jstroke, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_stroke_state *stroke = from_StrokeState(env, jstroke);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) return jni_throw_arg(env, "text must not be null");
	if (!stroke) return jni_throw_arg(env, "stroke must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_stroke_text(ctx, dev, text, stroke, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_ignoreText)(JNIEnv *env, jobject self, jobject jtext, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_text *text = from_Text(env, jtext);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!text) return jni_throw_arg(env, "text must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_ignore_text(ctx, dev, text, ctm);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillShade)(JNIEnv *env, jobject self, jobject jshd, jobject jctm, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_shade *shd = from_Shade(env, jshd);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!shd) return jni_throw_arg(env, "shade must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_shade(ctx, dev, shd, ctm, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillImage)(JNIEnv *env, jobject self, jobject jimg, jobject jctm, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_image *img = from_Image(env, jimg);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!img) return jni_throw_arg(env, "image must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_image(ctx, dev, img, ctm, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_fillImageMask)(JNIEnv *env, jobject self, jobject jimg, jobject jctm, jobject jcs, jfloatArray jcolor, jfloat alpha, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_image *img = from_Image(env, jimg);
	fz_matrix ctm = from_Matrix(env, jctm);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!img) return jni_throw_arg(env, "image must not be null");
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_fill_image_mask(ctx, dev, img, ctm, cs, color, alpha, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_clipImageMask)(JNIEnv *env, jobject self, jobject jimg, jobject jctm)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_image *img = from_Image(env, jimg);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!img) return jni_throw_arg(env, "image must not be null");

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_clip_image_mask(ctx, dev, img, ctm, fz_infinite_rect);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_popClip)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_pop_clip(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_beginLayer)(JNIEnv *env, jobject self, jstring jname)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	const char *name;
	int err;

	if (!ctx || !dev) return;

	if (jname)
	{
		name = (*env)->GetStringUTFChars(env, jname, NULL);
		if (!name) return;
	}

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_begin_layer(ctx, dev, name);
	fz_always(ctx)
	{
		(*env)->ReleaseStringUTFChars(env, jname, name);
		unlockNativeDevice(env, info);
	}
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endLayer)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_layer(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_beginMask)(JNIEnv *env, jobject self, jobject jrect, jboolean luminosity, jobject jcs, jfloatArray jcolor, jint jcp)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_rect rect = from_Rect(env, jrect);
	fz_colorspace *cs = from_ColorSpace(env, jcs);
	fz_color_params cp = from_ColorParams_safe(env, jcp);
	float color[FZ_MAX_COLORS];
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;
	if (!from_jfloatArray(env, color, cs ? fz_colorspace_n(ctx, cs) : FZ_MAX_COLORS, jcolor)) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_begin_mask(ctx, dev, rect, luminosity, cs, color, cp);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endMask)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_mask(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_beginGroup)(JNIEnv *env, jobject self, jobject jrect, jobject jcs, jboolean isolated, jboolean knockout, jint blendmode, jfloat alpha)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_rect rect = from_Rect(env, jrect);
	fz_colorspace *cs = from_ColorSpace(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_begin_group(ctx, dev, rect, cs, isolated, knockout, blendmode, alpha);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endGroup)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_group(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}

JNIEXPORT jint JNICALL
FUN(NativeDevice_beginTile)(JNIEnv *env, jobject self, jobject jarea, jobject jview, jfloat xstep, jfloat ystep, jobject jctm, jint id)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	fz_rect area = from_Rect(env, jarea);
	fz_rect view = from_Rect(env, jview);
	fz_matrix ctm = from_Matrix(env, jctm);
	NativeDeviceInfo *info;
	int i = 0;
	int err;

	if (!ctx || !dev) return 0;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return 0;
	fz_try(ctx)
		i = fz_begin_tile_id(ctx, dev, area, view, xstep, ystep, ctm, id);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx), 0;

	return i;
}

JNIEXPORT void JNICALL
FUN(NativeDevice_endTile)(JNIEnv *env, jobject self)
{
	fz_context *ctx = get_context(env);
	fz_device *dev = from_Device(env, self);
	NativeDeviceInfo *info;
	int err;

	if (!ctx || !dev) return;

	info = lockNativeDevice(env, self, &err);
	if (err)
		return;
	fz_try(ctx)
		fz_end_tile(ctx, dev);
	fz_always(ctx)
		unlockNativeDevice(env, info);
	fz_catch(ctx)
		return jni_rethrow(env, ctx);
}
