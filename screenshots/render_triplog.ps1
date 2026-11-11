$dest1 = "C:\Users\MY PC\Desktop\UniRoute Screenshots\fig_6_7_trip_log_file.png"
$dest2 = "c:\Users\MY PC\Desktop\bus-management\screenshots\fig_6_7_trip_log_file.png"
$dest3 = "C:\Users\MY PC\.gemini\antigravity\brain\1e755840-239f-40b3-8fbc-0b752c242587\fig_6_7_trip_log_file.png"

$csharpCode = @"
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;

public class TripLogRenderer {
    public static GraphicsPath RoundedRect(Rectangle bounds, int radius) {
        int diameter = radius * 2;
        Size size = new Size(diameter, diameter);
        Rectangle arc = new Rectangle(bounds.Location, size);
        GraphicsPath path = new GraphicsPath();
        path.AddArc(arc, 180, 90);
        arc.X = bounds.Right - diameter;
        path.AddArc(arc, 270, 90);
        arc.Y = bounds.Bottom - diameter;
        path.AddArc(arc, 0, 90);
        arc.X = bounds.Left;
        path.AddArc(arc, 90, 90);
        path.CloseFigure();
        return path;
    }

    public static void Render(string title, string[] lines, string outputPath, int totalWidth = 1140) {
        using (Font font = new Font("Consolas", 12f, FontStyle.Regular))
        using (Font boldFont = new Font("Consolas", 12f, FontStyle.Bold))
        using (Font titleFont = new Font("Segoe UI", 10f, FontStyle.Regular)) {
            int lineHeight = 26;
            int headerHeight = 44;
            int paddingX = 36;
            int paddingY = 24;
            int margin = 36;

            int termW = totalWidth - (margin * 2);
            int contentHeight = lines.Length * lineHeight;
            int terminalHeight = headerHeight + contentHeight + paddingY * 2;
            int totalHeight = terminalHeight + margin * 2;

            using (Bitmap bmp = new Bitmap(totalWidth, totalHeight))
            using (Graphics g = Graphics.FromImage(bmp)) {
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

                using (LinearGradientBrush bgBrush = new LinearGradientBrush(
                    new Rectangle(0, 0, totalWidth, totalHeight),
                    Color.FromArgb(20, 24, 38), Color.FromArgb(12, 14, 22), 65f)) {
                    g.FillRectangle(bgBrush, 0, 0, totalWidth, totalHeight);
                }

                int termX = margin;
                int termY = margin;
                int termH = terminalHeight;
                Rectangle termRect = new Rectangle(termX, termY, termW, termH);

                using (GraphicsPath shadowPath = RoundedRect(new Rectangle(termX + 4, termY + 8, termW, termH), 12))
                using (SolidBrush shadowBrush = new SolidBrush(Color.FromArgb(50, 0, 0, 0))) {
                    g.FillPath(shadowBrush, shadowPath);
                }

                using (GraphicsPath termPath = RoundedRect(termRect, 10)) {
                    using (SolidBrush termBg = new SolidBrush(Color.FromArgb(26, 29, 39))) {
                        g.FillPath(termBg, termPath);
                    }
                    using (Pen borderPen = new Pen(Color.FromArgb(55, 62, 82), 1.5f)) {
                        g.DrawPath(borderPen, termPath);
                    }
                }

                Rectangle headerRect = new Rectangle(termX, termY, termW, headerHeight);
                using (GraphicsPath headerPath = RoundedRect(headerRect, 10)) {
                    using (SolidBrush headerBg = new SolidBrush(Color.FromArgb(18, 20, 28))) {
                        g.SetClip(headerRect);
                        g.FillRectangle(headerBg, headerRect);
                        g.ResetClip();
                    }
                }
                using (Pen sepPen = new Pen(Color.FromArgb(42, 47, 63), 1f)) {
                    g.DrawLine(sepPen, termX, termY + headerHeight, termX + termW, termY + headerHeight);
                }

                int dotY = termY + 16;
                int dotSize = 12;
                using (SolidBrush red = new SolidBrush(Color.FromArgb(255, 95, 86)))
                    g.FillEllipse(red, termX + 20, dotY, dotSize, dotSize);
                using (SolidBrush yellow = new SolidBrush(Color.FromArgb(255, 189, 46)))
                    g.FillEllipse(yellow, termX + 40, dotY, dotSize, dotSize);
                using (SolidBrush green = new SolidBrush(Color.FromArgb(39, 201, 63)))
                    g.FillEllipse(green, termX + 60, dotY, dotSize, dotSize);

                using (SolidBrush titleBrush = new SolidBrush(Color.FromArgb(145, 155, 180))) {
                    SizeF titleSize = g.MeasureString(title, titleFont);
                    float titleX = termX + (termW - titleSize.Width) / 2;
                    g.DrawString(title, titleFont, titleBrush, titleX, termY + 13);
                }

                float textY = termY + headerHeight + paddingY;
                foreach (string line in lines) {
                    Color textColor = Color.FromArgb(226, 232, 240);
                    Font currentFont = font;

                    if (line.StartsWith("---")) {
                        textColor = Color.FromArgb(97, 175, 239);
                        currentFont = boldFont;
                    } else if (line.Contains("trip_log.txt - Notepad")) {
                        textColor = Color.FromArgb(255, 255, 255);
                        currentFont = boldFont;
                    } else if (line.Contains("File") || line.Contains("Edit")) {
                        textColor = Color.FromArgb(170, 180, 200);
                    } else if (line.Contains("Route:") || line.Contains("Status:")) {
                        textColor = Color.FromArgb(86, 182, 194);
                        currentFont = boldFont;
                    }

                    using (SolidBrush textBrush = new SolidBrush(textColor)) {
                        g.DrawString(line, currentFont, textBrush, termX + paddingX, textY);
                    }
                    textY += lineHeight;
                }

                bmp.Save(outputPath, System.Drawing.Imaging.ImageFormat.Png);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $csharpCode -ReferencedAssemblies System.Drawing

$l7 = @(
    "trip_log.txt - Notepad",
    "File   Edit   Format   View   Help",
    "---------------------------------------------------------------------------------------------------------",
    "Route: Route 1: City Center to GITS Campus | From: City Center | To: GITS Campus | Status: Active",
    "---------------------------------------------------------------------------------------------------------"
)

[TripLogRenderer]::Render("trip_log.txt - file storage", $l7, $dest2, 1140)

Copy-Item $dest2 -Destination $dest1 -Force
Copy-Item $dest2 -Destination $dest3 -Force

Get-Item $dest1 | Select-Object Name, Length, LastWriteTime
