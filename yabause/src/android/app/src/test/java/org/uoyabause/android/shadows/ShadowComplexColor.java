package org.uoyabause.android.shadows;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/**
 * Custom shadow for ComplexColor to avoid Jacoco bytecode instrumentation conflicts.
 * This shadow completely bypasses the instrumented ComplexColor class.
 */
@Implements(className = "android.content.res.ComplexColor")
public class ShadowComplexColor {
    
    private boolean isStateful = false;
    
    @Implementation
    public boolean isStateful() {
        return isStateful;
    }
    
    @Implementation
    public boolean canApplyTheme() {
        return false;
    }
    
    @Implementation
    public void applyTheme(Object theme) {
        // No-op implementation
    }
    
    public void setStateful(boolean stateful) {
        this.isStateful = stateful;
    }
}