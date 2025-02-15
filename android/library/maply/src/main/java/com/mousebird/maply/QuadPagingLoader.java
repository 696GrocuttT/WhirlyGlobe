package com.mousebird.maply;

import android.os.Handler;
import android.os.Looper;

import java.lang.ref.WeakReference;

/**
 * General purpose quad paging loader.
 * <br>
 * This quadtree based paging loader is for fetching and load general geometry.
 * There are other loaders that handle images and image animations.  This one is
 * purely for geometry.
 *
 * You need to fill in at least a MaplyLoaderInterpreter, which is probably your own
 * implementation.
 *
 * This replaces the QuadPagingLayer from WhirlyGlobe-Maply 2.x.
 */
public class QuadPagingLoader extends QuadLoaderBase {
    boolean valid = false;

    /**
     * Initialize with the objects needed to run.
     *
     * @param params Sampling params describe how the space is broken down into tiles.
     * @param tileInfo If fetching data remotely, this is the remote URL (and other info).  Can be null.
     * @param inInterp The loader interpreter takes input data (if any) per tile and turns it into visual objects
     * @param control The globe or map controller we're adding objects to.
     */
    public QuadPagingLoader(final SamplingParams params,TileInfoNew tileInfo,LoaderInterpreter inInterp,BaseController control)
    {
        this(params,new TileInfoNew[]{tileInfo},inInterp,control);
    }

    protected boolean noFetcher = false;

    /**
     * Initialize with the objects needed to run.
     *
     * @param params Sampling params describe how the space is broken down into tiles.
     * @param inInterp The loader interpreter takes input data (if any) per tile and turns it into visual objects
     * @param control The globe or map controller we're adding objects to.
     */
    public QuadPagingLoader(final SamplingParams params,LoaderInterpreter inInterp,BaseController control)
    {
        this(params,new TileInfoNew(params.getMinZoom(),params.getMaxZoom()),inInterp,control);
        noFetcher = true;
    }

    /**
     * Initialize with the objects needed to run.
     *
     * @param params Sampling params describe how the space is broken down into tiles.
     * @param inTileInfos If fetching data remotely, these are the remote URLs (and other info).  Can be null.
     * @param inInterp The loader interpreter takes input data (if any) per tile and turns it into visual objects
     * @param control The globe or map controller we're adding objects to.
     */
    public QuadPagingLoader(final SamplingParams params,TileInfoNew[] inTileInfos,LoaderInterpreter inInterp,BaseController control)
    {
        super(control, params, inTileInfos.length, Mode.Object);

        tileInfos = inTileInfos;
        loadInterp = inInterp;
        valid = true;

        if (control == null)
            return;
        // Let them change settings before we kick things off
        Handler handler = new Handler(Looper.getMainLooper());
        handler.post(() -> {
            if (valid) {
                delayedInit(params);
            }
        });
    }

    // Called a tick after creation to let users modify settings before we start
    public void delayedInit(final SamplingParams params) {
        if (tileFetcher == null && !noFetcher) {
            tileFetcher = getController().addTileFetcher("Image Fetcher");
        }

        samplingLayer = new WeakReference<>(getController().findSamplingLayer(params, this));
        loadInterp.setLoader(this);
    }

    protected LoaderReturn makeLoaderReturn() {
        return new ObjectLoaderReturn(this);
    }

    @Override
    public void shutdown() {
        valid = false;

        super.shutdown();
    }
}
