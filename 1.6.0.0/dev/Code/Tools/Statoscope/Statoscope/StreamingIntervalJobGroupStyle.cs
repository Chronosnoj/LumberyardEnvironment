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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace Statoscope
{
  class StreamingIntervalJobGroupStyle : IIntervalGroupStyle
  {
    private const float MaxPriority = 6.0f;
    private readonly Color[] m_stageColours = new Color[] { Color.Green, Color.Blue, Color.Magenta, Color.White };

    #region IIntervalGroupStyle Members

    public System.Drawing.Color GetIntervalColor(Interval iv)
    {
      StreamingInterval siv = (StreamingInterval)iv;
      switch (siv.Stage)
      {
        case StreamingStage.Waiting:
          {
            float greyScale = 0.75f - (siv.PerceptualImportance / MaxPriority) * 0.75f;
            return new RGB(greyScale, greyScale, greyScale).ConvertToSysDrawCol();
          }

        case StreamingStage.IO:
        case StreamingStage.Inflate:
        case StreamingStage.Async:
        case StreamingStage.IO_Inflate:
          return m_stageColours[(int)siv.Stage - 1];
        
        case StreamingStage.Preempted:
          return Color.Orange;

        case StreamingStage.Preempted_Inflate:
          return Color.DarkKhaki;
          
        default:
          return Color.Black;
      }
    }

    public int[] ReorderRails(IntervalTree tree, int[] rails, double selectionStart, double selectionEnd)
    {
      List<KeyValuePair<int, double>> railTimes = new List<KeyValuePair<int, double>>(rails.Length);

      long selectionStartUs = (long)(selectionStart * 1000000.0);
      long selectionEndUs = (long)(selectionEnd * 1000000.0);

      foreach (int railId in rails)
      {
        double t = double.MaxValue;

        bool railIntersectsSelection = false;

        foreach (var iv in tree.RangeEnum(railId, selectionStartUs, selectionEndUs))
        {
          railIntersectsSelection = true;
          break;
        }

        if (railIntersectsSelection)
        {
          foreach (var iv in tree.RangeEnum(railId, selectionStartUs, tree.MaxTimeUs))
          {
            StreamingInterval siv = iv as StreamingInterval;
            if ((siv.Stage & StreamingStage.IO) != 0)
            {
              t = Math.Min(t, iv.Start);
              break;
            }
          }
        }

        railTimes.Add(new KeyValuePair<int, double>(railId, t));
      }

      railTimes.Sort(new KeyValuePair_SecondComparer<int, double>());
      return railTimes.Select((x) => x.Key).ToArray<int>();
    }

    #endregion
  }
}
