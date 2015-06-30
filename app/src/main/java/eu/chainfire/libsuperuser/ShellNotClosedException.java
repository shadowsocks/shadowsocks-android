/*
 * Copyright (C) 2012-2014 Jorrit "Chainfire" Jongma
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

package eu.chainfire.libsuperuser;

/**
 * Exception class used to notify developer that a shell was not close()d
 */
@SuppressWarnings("serial")
public class ShellNotClosedException extends RuntimeException {
    public static final String EXCEPTION_NOT_CLOSED = "Application did not close() interactive shell";

    public ShellNotClosedException() {
        super(EXCEPTION_NOT_CLOSED);
    }
}
