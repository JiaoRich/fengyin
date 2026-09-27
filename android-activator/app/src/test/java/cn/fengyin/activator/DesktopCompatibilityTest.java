package cn.fengyin.activator;

import org.junit.Test;
import static org.junit.Assert.*;

/** Disposable JUCE-generated test key, never the production activation key. */
public class DesktopCompatibilityTest {
    @Test public void matchesDesktopGeneratorExactly() {
        String privateKey="4688ab6aa71f69103db53ad33550809e460d9b29abff497784c4618861bb7d45652cc7626beff4dea544f13d45e672dc45baaa36e9ef294ecce8c89301ac3f4343bc935cfdb603f23b322b89bf77de0d39ed21cb47b663d100beee12cea8b5c0296ef4c71953cd0f3f1b17b734285361b4d13995b76a8be7559c18ff922a38ed,b055ac8aa1ce86a89a4513100549418baf2203e82dfe37aacbeaf3d4f454b92d7ceff2760dd7e42c9d2c5b192ec01f26ae52a98948d5e7450045f56f842e9e29d82207736efd2d909550442677ff63a3cd531d51c497550491a651f00312ae198942ff575ceb44f06fd17170d7fb160dc4b3b90fffcf4883b5a162b9dbc68807";
        String publicKey="5,b055ac8aa1ce86a89a4513100549418baf2203e82dfe37aacbeaf3d4f454b92d7ceff2760dd7e42c9d2c5b192ec01f26ae52a98948d5e7450045f56f842e9e29d82207736efd2d909550442677ff63a3cd531d51c497550491a651f00312ae198942ff575ceb44f06fd17170d7fb160dc4b3b90fffcf4883b5a162b9dbc68807";
        String desktop="RlkxfEExQjJDM0Q0RTVGNjA3MTgyOTMwfEZZLUFORFJPSUQtR09MREVOfFBFUk1BTkVOVA==.5efb073e12a2319e96a27b35e7bee857c122a0aac03ec0978cf5ede219db07bcac7e574b9dbe89e6ca8ed365050b4f8bcf92a9b12b0005161ce86d394431bc7cceb47471fd1ff4156b74513ad02f7cd3f4d19aa7b89ee0b5b025fc7797c03272d3f6ac17ab63e829ce49bc6c9b0eb155f067849dcf8d7446b3a0eef2228619ad";
        assertEquals(desktop,LicenseCodec.generate("A1B2C3D4E5F607182930","FY-ANDROID-GOLDEN",privateKey));
        assertTrue(LicenseCodec.verify(desktop,"A1B2C3D4E5F607182930",publicKey));
        assertFalse(LicenseCodec.verify(desktop,"A1B2C3D4E5F607182930",LicenseCodec.PUBLIC_KEY));
    }
}

