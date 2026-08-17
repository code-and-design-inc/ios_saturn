/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
package org.uoyabause.android

import android.content.Context
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import android.util.Log
import androidx.documentfile.provider.DocumentFile

/**
 * Utility class for handling Storage Access Framework (SAF) URI permissions.
 *
 * When receiving content URIs from external sources (like other apps via Intent),
 * direct access may fail due to permission issues. This helper provides methods
 * to access files using existing tree permissions.
 */
object UriPermissionHelper {
    private const val TAG = "UriPermissionHelper"

    /**
     * Convert a tree/document URI to a simple document URI.
     *
     * Tree document URIs have the format:
     *   content://authority/tree/{treeDocId}/document/{docId}
     *
     * Simple document URIs have the format:
     *   content://authority/document/{docId}
     *
     * This function converts the former to the latter for database lookups.
     *
     * @param uri The URI to convert
     * @return The simple document URI, or the original URI if conversion is not applicable
     */
    fun toSimpleDocumentUri(uri: Uri): Uri {
        val uriString = uri.toString()

        // Check if this is a tree/document URI pattern
        val treeDocPattern = Regex("""(content://[^/]+)/tree/[^/]+/document/(.+)""")
        val match = treeDocPattern.find(uriString)

        return if (match != null) {
            val authority = match.groupValues[1]
            val documentId = match.groupValues[2]
            val simpleUri = "$authority/document/$documentId"
            Log.d(TAG, "Converted tree URI to simple document URI: $simpleUri")
            Uri.parse(simpleUri)
        } else {
            uri
        }
    }

    /**
     * Extract the document ID from a URI.
     *
     * @param uri The URI to extract from
     * @return The document ID, or null if extraction fails
     */
    fun extractDocumentId(uri: Uri): String? = try {
        DocumentsContract.getDocumentId(uri)
    } catch (e: Exception) {
        Log.d(TAG, "Could not extract document ID from URI: $uri")
        null
    }

    /**
     * Find a persisted tree URI that contains the given document URI.
     *
     * @param context The context to use for accessing content resolver
     * @param documentUri The document URI to find a tree for
     * @return The tree URI if found, null otherwise
     */
    fun findTreeUriForDocument(
        context: Context,
        documentUri: Uri,
    ): Uri? {
        val persistedUris = context.contentResolver.persistedUriPermissions

        val documentId =
            try {
                DocumentsContract.getDocumentId(documentUri)
            } catch (e: Exception) {
                Log.d(TAG, "Could not get document ID from URI: $documentUri")
                return null
            }

        for (permission in persistedUris) {
            if (!permission.isReadPermission) continue
            if (!DocumentsContract.isTreeUri(permission.uri)) continue

            try {
                val treeDocumentId = DocumentsContract.getTreeDocumentId(permission.uri)
                if (documentId.startsWith(treeDocumentId)) {
                    Log.d(TAG, "Found matching tree URI: ${permission.uri}")
                    return permission.uri
                }
            } catch (e: Exception) {
                Log.d(TAG, "Could not check tree URI: ${permission.uri}, error: ${e.message}")
                continue
            }
        }
        Log.d(TAG, "No matching tree URI found for document: $documentUri")
        return null
    }

    /**
     * Open a document URI using tree permission.
     *
     * This method navigates through the tree structure to find the file
     * and opens it using the tree's permission rather than direct access.
     *
     * @param context The context to use for accessing content resolver
     * @param documentUri The document URI to open
     * @param treeUri The tree URI that has permission to access the document
     * @return ParcelFileDescriptor if successful, null otherwise
     */
    fun openDocumentViaTree(
        context: Context,
        documentUri: Uri,
        treeUri: Uri,
    ): ParcelFileDescriptor? {
        val treeDocument = DocumentFile.fromTreeUri(context, treeUri)
        if (treeDocument == null) {
            Log.e(TAG, "Could not create DocumentFile from tree URI: $treeUri")
            return null
        }

        try {
            // Get file name from document URI
            val documentId = DocumentsContract.getDocumentId(documentUri)
            val treeDocumentId = DocumentsContract.getTreeDocumentId(treeUri)
            val relativePath = documentId.removePrefix(treeDocumentId).trim('/')

            Log.d(TAG, "Navigating to relative path: $relativePath")

            // Navigate to the file
            var current: DocumentFile = treeDocument
            val pathParts = relativePath.split("/").filter { it.isNotEmpty() }

            for (part in pathParts) {
                val decodedPart = Uri.decode(part)
                val found = current.findFile(decodedPart)
                if (found == null) {
                    Log.e(TAG, "Could not find file part: $decodedPart in ${current.uri}")
                    return null
                }
                current = found
            }

            Log.d(TAG, "Found file via tree navigation: ${current.uri}")
            return context.contentResolver.openFileDescriptor(current.uri, "r")
        } catch (e: Exception) {
            Log.e(TAG, "Error opening document via tree: ${e.message}")
            return null
        }
    }
}
