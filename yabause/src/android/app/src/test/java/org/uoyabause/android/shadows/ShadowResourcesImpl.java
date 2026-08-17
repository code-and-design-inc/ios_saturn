package org.uoyabause.android.shadows;

import android.content.res.Resources;
import android.util.TypedValue;
import android.graphics.drawable.ColorDrawable;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/**
 * Custom shadow for ResourcesImpl to avoid Jacoco bytecode instrumentation conflicts.
 * This focuses specifically on the loadComplexColorForCookie method that causes issues.
 */
@Implements(className = "android.content.res.ResourcesImpl")
public class ShadowResourcesImpl {
    
    @Implementation
    public Object loadComplexColorForCookie(Resources wrapper, TypedValue value, int id, Resources.Theme theme) {
        // Return a simple ColorDrawable to avoid the complex bytecode verification
        // This is a safe fallback that prevents the verification error
        return new ColorDrawable(0xFF000000); // Default to black
    }
}