/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

package com.amazon.lumberyard;

import android.app.Activity;
import android.content.res.Resources;
import android.os.Bundle;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;

import java.util.List;
import java.util.ArrayList;

import org.libsdl.app.SDLActivity;


////////////////////////////////////////////////////////////////
public class LumberyardActivity extends SDLActivity
{
    ////////////////////////////////////////////////////////////////
    // called from the native during startup to get the game project dll and asset directory
    public String GetGameProjectName()
    {
        Resources resources = this.getResources();
        int stringId = resources.getIdentifier("project_name", "string", this.getPackageName());
        return resources.getString(stringId);
    }

    ////////////////////////////////////////////////////////////////
    // called from the natvie code, either right after the renderer is initialized with a splash
    // screen of it's own, or at the end of the system initialization when no native splash
    // is specified
    public void OnEngineRendererTakeover()
    {
        this.runOnUiThread(new Runnable() {
            @Override
            public void run()
            {
                mLayout.removeView(m_splashView);
                m_splashView = null;
            }
        });
    }

    ////////////////////////////////////////////////////////////////
    public void RegisterActivityResultsListener(ActivityResultsListener listener)
    {
        m_activityResultsListeners.add(listener);
    }


    ////////////////////////////////////////////////////////////////
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        Resources resources = this.getResources();
        int layoutId = resources.getIdentifier("splash_screen", "layout", this.getPackageName());

        LayoutInflater factory = LayoutInflater.from(this);
        m_splashView = factory.inflate(layoutId, null);
        mLayout.addView(m_splashView);
    }

    ////////////////////////////////////////////////////////////////
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        for (ActivityResultsListener listener : m_activityResultsListeners)
        {
            listener.ProcessActivityResult(requestCode, resultCode, data);
        }
    }


    private View m_splashView = null;
    private List<ActivityResultsListener> m_activityResultsListeners = new ArrayList<ActivityResultsListener>();
}