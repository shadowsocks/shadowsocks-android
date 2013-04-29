/* Shadowsocks - A shadowsocks client for Android
 * Copyright (C) 2012 <max.c.lv@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *                            ___====-_  _-====___
 *                      _--^^^#####//      \\#####^^^--_
 *                   _-^##########// (    ) \\##########^-_
 *                  -############//  |\^^/|  \\############-
 *                _/############//   (@::@)   \\############\_
 *               /#############((     \\//     ))#############\
 *              -###############\\    (oo)    //###############-
 *             -#################\\  / VV \  //#################-
 *            -###################\\/      \//###################-
 *           _#/|##########/\######(   /\   )######/\##########|\#_
 *           |/ |#/\#/\#/\/  \#/\##\  |  |  /##/\#/  \/\#/\#/\#| \|
 *           `  |/  V  V  `   V  \#\| |  | |/#/  V   '  V  V  \|  '
 *              `   `  `      `   / | |  | | \   '      '  '   '
 *                               (  | |  | |  )
 *                              __\ | |  | | /__
 *                             (vvv(VVV)(VVV)vvv)
 *
 *                              HERE BE DRAGONS
 *
 */

package com.github.shadowsocks;

import android.R;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.widget.ImageView;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.HashMap;
import java.util.Stack;

public class ImageLoader {

  final int stub_id = R.drawable.sym_def_app_icon;
  PhotosQueue photosQueue = new PhotosQueue();
  PhotosLoader photoLoaderThread = new PhotosLoader();
  // the simplest in-memory cache implementation. This should be replaced with
  // something like SoftReference or BitmapOptions.inPurgeable(since 1.6)
  private HashMap<Integer, Bitmap> cache = new HashMap<Integer, Bitmap>();
  private File cacheDir;
  private Context context;

  public ImageLoader(Context c) {
    // Make the background thead low priority. This way it will not affect
    // the UI performance
    photoLoaderThread.setPriority(Thread.NORM_PRIORITY - 1);

    context = c;

    // Find the dir to save cached images
    cacheDir = context.getCacheDir();
  }

  public void clearCache() {
    // clear memory cache
    cache.clear();

    // clear SD cache
    File[] files = cacheDir.listFiles();
    for (File f : files)
      f.delete();
  }

  // decodes image and scales it to reduce memory consumption
  private Bitmap decodeFile(File f) {
    try {
      // decode image size
      BitmapFactory.Options o = new BitmapFactory.Options();
      o.inJustDecodeBounds = true;
      BitmapFactory.decodeStream(new FileInputStream(f), null, o);

      // Find the correct scale value. It should be the power of 2.
      final int REQUIRED_SIZE = 70;
      int width_tmp = o.outWidth, height_tmp = o.outHeight;
      int scale = 1;
      while (true) {
        if (width_tmp / 2 < REQUIRED_SIZE || height_tmp / 2 < REQUIRED_SIZE) break;
        width_tmp /= 2;
        height_tmp /= 2;
        scale *= 2;
      }

      // decode with inSampleSize
      BitmapFactory.Options o2 = new BitmapFactory.Options();
      o2.inSampleSize = scale;
      return BitmapFactory.decodeStream(new FileInputStream(f), null, o2);
    } catch (FileNotFoundException ignored) {
    }
    return null;
  }

  public void DisplayImage(int uid, Activity activity, ImageView imageView) {
    if (cache.containsKey(uid)) {
      imageView.setImageBitmap(cache.get(uid));
    } else {
      queuePhoto(uid, activity, imageView);
      imageView.setImageResource(stub_id);
    }
  }

  private Bitmap getBitmap(int uid) {
    // I identify images by hashcode. Not a perfect solution, good for the
    // demo.
    String filename = String.valueOf(uid);
    File f = new File(cacheDir, filename);

    // from SD cache
    Bitmap b = decodeFile(f);
    if (b != null) return b;

    // from web
    try {
      BitmapDrawable icon = (BitmapDrawable) Utils.getAppIcon(context, uid);
      return icon.getBitmap();
    } catch (Exception ex) {
      return null;
    }
  }

  private void queuePhoto(int uid, Activity activity, ImageView imageView) {
    // This ImageView may be used for other images before. So there may be
    // some old tasks in the queue. We need to discard them.
    photosQueue.Clean(imageView);
    PhotoToLoad p = new PhotoToLoad(uid, imageView);
    synchronized (photosQueue.photosToLoad) {
      photosQueue.photosToLoad.push(p);
      photosQueue.photosToLoad.notifyAll();
    }

    // start thread if it's not started yet
    if (photoLoaderThread.getState() == Thread.State.NEW) photoLoaderThread.start();
  }

  public void stopThread() {
    photoLoaderThread.interrupt();
  }

  // Used to display bitmap in the UI thread
  class BitmapDisplayer implements Runnable {
    Bitmap bitmap;
    ImageView imageView;

    public BitmapDisplayer(Bitmap b, ImageView i) {
      bitmap = b;
      imageView = i;
    }

    @Override
    public void run() {
      if (bitmap != null) {
        imageView.setImageBitmap(bitmap);
      } else {
        imageView.setImageResource(stub_id);
      }
    }
  }

  class PhotosLoader extends Thread {
    @Override
    public void run() {
      try {
        while (true) {
          // thread waits until there are any images to load in the
          // queue
          if (photosQueue.photosToLoad.size() == 0) {
            synchronized (photosQueue.photosToLoad) {
              photosQueue.photosToLoad.wait();
            }
          }
          if (photosQueue.photosToLoad.size() != 0) {
            PhotoToLoad photoToLoad;
            synchronized (photosQueue.photosToLoad) {
              photoToLoad = photosQueue.photosToLoad.pop();
            }
            Bitmap bmp = getBitmap(photoToLoad.uid);
            cache.put(photoToLoad.uid, bmp);
            Object tag = photoToLoad.imageView.getTag();
            if (tag != null && ((Integer) tag) == photoToLoad.uid) {
              BitmapDisplayer bd = new BitmapDisplayer(bmp, photoToLoad.imageView);
              Activity a = (Activity) photoToLoad.imageView.getContext();
              a.runOnUiThread(bd);
            }
          }
          if (Thread.interrupted()) break;
        }
      } catch (InterruptedException e) {
        // allow thread to exit
      }
    }
  }

  // stores list of photos to download
  class PhotosQueue {
    private final Stack<PhotoToLoad> photosToLoad = new Stack<PhotoToLoad>();

    // removes all instances of this ImageView
    public void Clean(ImageView image) {
      synchronized (photosToLoad) {
        for (int j = 0; j < photosToLoad.size(); ) {
          if (photosToLoad.get(j).imageView == image) {
            photosToLoad.remove(j);
          } else {
            ++j;
          }
        }
      }
    }
  }

  // Task for the queue
  private class PhotoToLoad {
    public int uid;
    public ImageView imageView;

    public PhotoToLoad(int u, ImageView i) {
      uid = u;
      imageView = i;
    }
  }
}
