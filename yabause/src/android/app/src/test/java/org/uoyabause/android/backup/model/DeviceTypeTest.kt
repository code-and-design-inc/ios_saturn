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

import org.junit.Assert.assertEquals
import org.junit.Test

/**
 * Unit tests for DeviceType enum
 * Test cases: UT-M01 ~ UT-M05
 */
class DeviceTypeTest {
    // UT-M01: Test fromId conversion with valid IDs
    @Test
    fun `fromId returns INTERNAL for id 0`() {
        assertEquals(DeviceType.INTERNAL, DeviceType.fromId(0))
    }

    @Test
    fun `fromId returns EXTERNAL for id 1`() {
        assertEquals(DeviceType.EXTERNAL, DeviceType.fromId(1))
    }

    @Test
    fun `fromId returns CLOUD for id 48`() {
        assertEquals(DeviceType.CLOUD, DeviceType.fromId(48))
    }

    @Test
    fun `fromId returns SHARED for id -1`() {
        assertEquals(DeviceType.SHARED, DeviceType.fromId(-1))
    }

    // UT-M02: Test fromId with unknown ID returns INTERNAL as default
    @Test
    fun `fromId returns INTERNAL for unknown id`() {
        assertEquals(DeviceType.INTERNAL, DeviceType.fromId(999))
        assertEquals(DeviceType.INTERNAL, DeviceType.fromId(-999))
        assertEquals(DeviceType.INTERNAL, DeviceType.fromId(100))
    }

    // UT-M03: Test fromTabPosition conversion
    @Test
    fun `fromTabPosition returns INTERNAL for position 0`() {
        assertEquals(DeviceType.INTERNAL, DeviceType.fromTabPosition(0))
    }

    @Test
    fun `fromTabPosition returns EXTERNAL for position 1`() {
        assertEquals(DeviceType.EXTERNAL, DeviceType.fromTabPosition(1))
    }

    @Test
    fun `fromTabPosition returns CLOUD for position 2`() {
        assertEquals(DeviceType.CLOUD, DeviceType.fromTabPosition(2))
    }

    @Test
    fun `fromTabPosition returns SHARED for position 3`() {
        assertEquals(DeviceType.SHARED, DeviceType.fromTabPosition(3))
    }

    // UT-M04: Test fromTabPosition with invalid position returns INTERNAL
    @Test
    fun `fromTabPosition returns INTERNAL for invalid position`() {
        assertEquals(DeviceType.INTERNAL, DeviceType.fromTabPosition(-1))
        assertEquals(DeviceType.INTERNAL, DeviceType.fromTabPosition(4))
        assertEquals(DeviceType.INTERNAL, DeviceType.fromTabPosition(100))
    }

    // UT-M05: Test displayName values
    @Test
    fun `displayName returns correct values`() {
        assertEquals("Internal", DeviceType.INTERNAL.displayName)
        assertEquals("External", DeviceType.EXTERNAL.displayName)
        assertEquals("Cloud", DeviceType.CLOUD.displayName)
        assertEquals("Shared", DeviceType.SHARED.displayName)
    }

    // Test id values
    @Test
    fun `id returns correct values`() {
        assertEquals(0, DeviceType.INTERNAL.id)
        assertEquals(1, DeviceType.EXTERNAL.id)
        assertEquals(48, DeviceType.CLOUD.id)
        assertEquals(-1, DeviceType.SHARED.id)
    }
}
