/*
 * Copyright 2024 devMiyax
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.uoyabause.android.backup.model

/**
 * Sort modes for local and cloud backup lists.
 */
enum class BackupSortMode {
    /** Sort by filename A-Z */
    NAME_ASC,

    /** Sort by filename Z-A */
    NAME_DESC,

    /** Sort by save date, newest first (default) */
    DATE_DESC,

    /** Sort by save date, oldest first */
    DATE_ASC,
}
