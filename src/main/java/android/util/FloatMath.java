/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.util;

/**
 * Math routines similar to those found in {@link java.lang.Math}.
 *
 * <p>Historically these methods were faster than the equivalent double-based
 * {@link java.lang.Math} methods. On versions of Android with a JIT they
 * became slower and have since been re-implemented to wrap calls to
 * {@link java.lang.Math}. {@link java.lang.Math} should be used in
 * preference.
 *
 * @deprecated Use {@link java.lang.Math} instead.
 */
@Deprecated
public class FloatMath {

    /** Prevents instantiation. */
    private FloatMath() {}

    /**
     * Returns the float conversion of the most positive (i.e. closest to
     * positive infinity) integer value which is less than the argument.
     *
     * @param value to be converted
     * @return the floor of value
     */
    public static float floor(float value) {
        return (float) Math.floor(value);
    }

    /**
     * Returns the float conversion of the most negative (i.e. closest to
     * negative infinity) integer value which is greater than the argument.
     *
     * @param value to be converted
     * @return the ceiling of value
     */
    public static float ceil(float value) {
        return (float) Math.ceil(value);
    }

    /**
     * Returns the closest float approximation of the sine of the argument.
     *
     * @param angle to compute the cosine of, in radians
     * @return the sine of angle
     */
    public static float sin(float angle) {
        return (float) Math.sin(angle);
    }

    /**
     * Returns the closest float approximation of the cosine of the argument.
     *
     * @param angle to compute the cosine of, in radians
     * @return the cosine of angle
     */
    public static float cos(float angle) {
        return (float) Math.cos(angle);
    }

    /**
     * Returns the closest float approximation of the square root of the
     * argument.
     *
     * @param value to compute sqrt of
     * @return the square root of value
     */
    public static float sqrt(float value) {
        return (float) Math.sqrt(value);
    }

    /**
     * Returns the closest float approximation of the raising "e" to the power
     * of the argument.
     *
     * @param value to compute the exponential of
     * @return the exponential of value
     */
    public static float exp(float value) {
        return (float) Math.exp(value);
    }

    /**
     * Returns the closest float approximation of the result of raising {@code
     * x} to the power of {@code y}.
     *
     * @param x the base of the operation.
     * @param y the exponent of the operation.
     * @return {@code x} to the power of {@code y}.
     */
    public static float pow(float x, float y) {
        return (float) Math.pow(x, y);
    }

    /**
     * Returns {@code sqrt(}<i>{@code x}</i><sup>{@code 2}</sup>{@code +} <i>
     * {@code y}</i><sup>{@code 2}</sup>{@code )}.
     *
     * @param x a float number
     * @param y a float number
     * @return the hypotenuse
     */
    public static float hypot(float x, float y) {
        return (float) Math.hypot(x, y);
    }
}
