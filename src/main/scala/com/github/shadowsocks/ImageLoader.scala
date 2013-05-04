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
package com.github.shadowsocks

import android.app.Activity
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.BitmapDrawable
import android.widget.ImageView
import java.io.File
import java.io.FileInputStream
import java.io.FileNotFoundException
import scala.collection.mutable

object ImageLoader {
  def getImageLoader(context: Context): ImageLoader = {
    if (il == null) {
      il = new ImageLoader(context)
    }
    il
  }

  class PhotosQueue {
    def clean(image: ImageView) {
      photosToLoad synchronized {
        {
          photosToLoad filterNot (photo => photo.imageView eq image)
        }
      }
    }

    val photosToLoad: mutable.Stack[PhotoToLoad] = new mutable.Stack[PhotoToLoad]
  }

  class PhotoToLoad {
    def this(u: Int, i: ImageView) {
      this()
      uid = u
      imageView = i
    }

    var uid: Int = 0
    var imageView: ImageView = null
  }

  class BitmapDisplayer(bitmap: Bitmap, imageView: ImageView) extends Runnable {
    def run() {
      if (bitmap != null) {
        imageView.setImageBitmap(bitmap)
      }
      else {
        imageView.setImageResource(stub_id)
      }
    }
  }

  var il: ImageLoader = null
  val stub_id: Int = android.R.drawable.sym_def_app_icon
}

class ImageLoader {
  def this(c: Context) {
    this()
    photoLoaderThread.setPriority(Thread.NORM_PRIORITY - 1)
    context = c
    cacheDir = context.getCacheDir
  }

  def clearCache() {
    cache.clear()
    val files: Array[File] = cacheDir.listFiles
    for (f <- files) f.delete
  }

  private def decodeFile(f: File): Bitmap = {
    try {
      val o: BitmapFactory.Options = new BitmapFactory.Options
      o.inJustDecodeBounds = true
      BitmapFactory.decodeStream(new FileInputStream(f), null, o)
      val REQUIRED_SIZE: Int = 70
      var width_tmp: Int = o.outWidth
      var height_tmp: Int = o.outHeight
      var scale: Int = 1
      while (true) {
        if (width_tmp / 2 >= REQUIRED_SIZE && height_tmp / 2 >= REQUIRED_SIZE) {
          width_tmp /= 2
          height_tmp /= 2
          scale *= 2
        }
      }
      val o2: BitmapFactory.Options = new BitmapFactory.Options
      o2.inSampleSize = scale
      return BitmapFactory.decodeStream(new FileInputStream(f), null, o2)
    }
    catch {
      case ignored: FileNotFoundException => {
      }
    }
    null
  }

  def DisplayImage(uid: Int, activity: Activity, imageView: ImageView) {
    cache.get(uid) map {
      bitmap => imageView.setImageBitmap(bitmap)
    } getOrElse {
      queuePhoto(uid, activity, imageView)
      imageView.setImageResource(ImageLoader.stub_id)
    }
  }

  private def getBitmap(uid: Int): Bitmap = {
    val filename = String.valueOf(uid)
    val f: File = new File(cacheDir, filename)
    val b: Bitmap = decodeFile(f)
    if (b != null) return b
    try {
      val icon: BitmapDrawable = Utils.getAppIcon(context, uid).asInstanceOf[BitmapDrawable]
      icon.getBitmap
    }
    catch {
      case ex: Exception => {
        return null
      }
    }
  }

  def queuePhoto(uid: Int, activity: Activity, imageView: ImageView) {
    photosQueue.clean(imageView)
    val p: ImageLoader.PhotoToLoad = new ImageLoader.PhotoToLoad(uid, imageView)
    photosQueue.photosToLoad synchronized {
      photosQueue.photosToLoad.push(p)
      photosQueue.photosToLoad.notifyAll()
    }
    if (photoLoaderThread.getState eq Thread.State.NEW) photoLoaderThread.start()
  }

  def stopThread() {
    photoLoaderThread.interrupt()
  }

  val photosQueue: ImageLoader.PhotosQueue = new ImageLoader.PhotosQueue
  val photoLoaderThread: PhotosLoader = new PhotosLoader
  var cache: mutable.HashMap[Int, Bitmap] = new mutable.HashMap[Int, Bitmap]
  var cacheDir: File = null
  var context: Context = null

  class PhotosLoader extends Thread {
    override def run() {
      try {
        while (true) {
          if (photosQueue.photosToLoad.size == 0) {
            photosQueue.photosToLoad synchronized {
              photosQueue.photosToLoad.wait()
            }
          }
          if (photosQueue.photosToLoad.size != 0) {
            var photoToLoad: ImageLoader.PhotoToLoad = null
            photosQueue.photosToLoad synchronized {
              photoToLoad = photosQueue.photosToLoad.pop()
            }
            val bmp: Bitmap = getBitmap(photoToLoad.uid)
            cache.put(photoToLoad.uid, bmp)
            val tag: AnyRef = photoToLoad.imageView.getTag
            if (tag != null && tag.asInstanceOf[Int] == photoToLoad.uid) {
              val bd: ImageLoader.BitmapDisplayer = new ImageLoader.BitmapDisplayer(bmp, photoToLoad.imageView)
              val a: Activity = photoToLoad.imageView.getContext.asInstanceOf[Activity]
              a.runOnUiThread(bd)
            }
          }
          if (Thread.interrupted) return
        }
      }
      catch {
        case e: InterruptedException => {
        }
      }
    }
  }
}