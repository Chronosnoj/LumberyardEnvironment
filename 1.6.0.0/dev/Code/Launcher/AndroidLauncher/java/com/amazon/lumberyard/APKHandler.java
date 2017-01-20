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

import android.content.res.AssetManager;
import android.util.Log;
import java.io.IOException;
import android.app.Activity;

////////////////////////////////////////////////////////////////////////////////////////////////////

public class APKHandler
{
    public static String[] GetFilesAndDirectoriesInPath(Activity act, String path)
    {
        AssetManager am = act.getAssets();
        String[] filelist = {};

        try
        {
            filelist = am.list(path);
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
        finally
        {
            return filelist;
        }
    }

    public static boolean IsDirectory(Activity act, String path)
    {
        AssetManager am = act.getAssets();
        String[] filelist = {};

        boolean retVal = false;

        try
        {
            filelist = am.list(path);

            if(filelist.length > 0)
            {
                retVal = true;
            }
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
        finally
        {
            return retVal;
        }
    }

}