-keepattributes InnerClasses
-allowaccessmodification
-repackageclasses
-keepclassmembers class * implements android.os.Parcelable {
    public static final ** CREATOR;
}