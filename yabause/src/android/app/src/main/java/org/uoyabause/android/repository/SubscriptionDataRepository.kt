/*
 * Copyright 2022 Google, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.uoyabause.android.repository

import com.android.billingclient.api.ProductDetails
import com.android.billingclient.api.Purchase
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import org.uoyabause.android.billing.BillingClientWrapper

/**
 * The [SubscriptionDataRepository] processes and tranforms the [StateFlow] data received from
 * the [BillingClientWrapper] into [Flow] data available to the viewModel.
 *
 */
class SubscriptionDataRepository(
    billingClientWrapper: BillingClientWrapper,
) {
    // Set to true when a returned purchase is an auto-renewing pro annual subscription.
    val hasRenewableProAnnual: Flow<Boolean> =
        billingClientWrapper.purchases.map { purchaseList ->
            purchaseList.any { purchase ->
                purchase.products.contains(PRO_ANNUAL_SUB) && purchase.isAutoRenewing
            }
        }

    // Set to true when a returned purchase is a prepaid pro annual subscription.
    val hasPrepaidProAnnual: Flow<Boolean> =
        billingClientWrapper.purchases.map { purchaseList ->
            purchaseList.any { purchase ->
                !purchase.isAutoRenewing && purchase.products.contains(PRO_ANNUAL_SUB)
            }
        }

    // ProductDetails for the pro annual subscription.
    val proAnnualProductDetails: Flow<ProductDetails> =
        billingClientWrapper.productWithProductDetails
            .filter {
                it.containsKey(
                    PRO_ANNUAL_SUB,
                )
            }.map { it[PRO_ANNUAL_SUB]!! }

    // Set to true when the user has any active pro annual subscription.
    // Validates purchase state, acknowledgement status, and non-empty purchase token.
    val hasProAnnual: Flow<Boolean> =
        billingClientWrapper.purchases.map { purchaseList ->
            purchaseList.any { purchase ->
                purchase.products.contains(PRO_ANNUAL_SUB) &&
                    purchase.purchaseState == Purchase.PurchaseState.PURCHASED &&
                    purchase.isAcknowledged &&
                    purchase.purchaseToken.isNotEmpty()
            }
        }

    // List of current purchases returned by the Google PLay Billing client library.
    val purchases: Flow<List<Purchase>> = billingClientWrapper.purchases

    // Set to true when a purchase is acknowledged.
    val isNewPurchaseAcknowledged: Flow<Boolean> = billingClientWrapper.isNewPurchaseAcknowledged

    companion object {
        private const val PRO_ANNUAL_SUB = BillingClientWrapper.PRO_ANNUAL_SUB
    }
}
