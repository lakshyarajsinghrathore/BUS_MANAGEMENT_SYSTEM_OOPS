$desktopOut = "C:\Users\MY PC\Desktop\UniRoute Screenshots"
$projOut = "c:\Users\MY PC\Desktop\bus-management\screenshots"
$brainOut = "C:\Users\MY PC\.gemini\antigravity\brain\1e755840-239f-40b3-8fbc-0b752c242587"

New-Item -ItemType Directory -Path $desktopOut -Force | Out-Null
New-Item -ItemType Directory -Path $projOut -Force | Out-Null

$csharpCode = @"
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;

public class CarbonStyler {
    public static GraphicsPath RoundedRect(Rectangle bounds, int radius) {
        int diameter = radius * 2;
        Size size = new Size(diameter, diameter);
        Rectangle arc = new Rectangle(bounds.Location, size);
        GraphicsPath path = new GraphicsPath();

        if (radius == 0) {
            path.AddRectangle(bounds);
            return path;
        }

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

    public static void RenderTerminal(string title, string[] lines, string outputPath, int width = 940) {
        using (Font font = new Font("Consolas", 12f, FontStyle.Regular))
        using (Font boldFont = new Font("Consolas", 12f, FontStyle.Bold))
        using (Font titleFont = new Font("Segoe UI", 10f, FontStyle.Regular)) {
            int lineHeight = 24;
            int headerHeight = 44;
            int paddingX = 30;
            int paddingY = 24;

            int contentHeight = lines.Length * lineHeight;
            int terminalHeight = headerHeight + contentHeight + paddingY * 2;
            int margin = 36;
            int totalWidth = width;
            int totalHeight = terminalHeight + margin * 2;

            using (Bitmap bmp = new Bitmap(totalWidth, totalHeight))
            using (Graphics g = Graphics.FromImage(bmp)) {
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

                // Outer Canvas Gradient (Modern Dark Blue / Violet gradient)
                using (LinearGradientBrush bgBrush = new LinearGradientBrush(
                    new Rectangle(0, 0, totalWidth, totalHeight),
                    Color.FromArgb(20, 24, 38), Color.FromArgb(12, 14, 22), 65f)) {
                    g.FillRectangle(bgBrush, 0, 0, totalWidth, totalHeight);
                }

                int termX = margin;
                int termY = margin;
                int termW = totalWidth - (margin * 2);
                int termH = terminalHeight;
                Rectangle termRect = new Rectangle(termX, termY, termW, termH);

                // Drop Shadow
                using (GraphicsPath shadowPath = RoundedRect(new Rectangle(termX + 4, termY + 8, termW, termH), 12))
                using (SolidBrush shadowBrush = new SolidBrush(Color.FromArgb(50, 0, 0, 0))) {
                    g.FillPath(shadowBrush, shadowPath);
                }

                // Terminal Body
                using (GraphicsPath termPath = RoundedRect(termRect, 10)) {
                    using (SolidBrush termBg = new SolidBrush(Color.FromArgb(26, 29, 39))) {
                        g.FillPath(termBg, termPath);
                    }
                    using (Pen borderPen = new Pen(Color.FromArgb(55, 62, 82), 1.5f)) {
                        g.DrawPath(borderPen, termPath);
                    }
                }

                // Header Background
                Rectangle headerRect = new Rectangle(termX, termY, termW, headerHeight);
                using (GraphicsPath headerPath = RoundedRect(headerRect, 10)) {
                    using (SolidBrush headerBg = new SolidBrush(Color.FromArgb(18, 20, 28))) {
                        // clip to top half so bottom corners don't round
                        g.SetClip(headerRect);
                        g.FillRectangle(headerBg, headerRect);
                        g.ResetClip();
                    }
                }
                using (Pen sepPen = new Pen(Color.FromArgb(42, 47, 63), 1f)) {
                    g.DrawLine(sepPen, termX, termY + headerHeight, termX + termW, termY + headerHeight);
                }

                // Window Traffic Lights
                int dotY = termY + 16;
                int dotSize = 12;
                using (SolidBrush red = new SolidBrush(Color.FromArgb(255, 95, 86)))
                    g.FillEllipse(red, termX + 20, dotY, dotSize, dotSize);
                using (SolidBrush yellow = new SolidBrush(Color.FromArgb(255, 189, 46)))
                    g.FillEllipse(yellow, termX + 40, dotY, dotSize, dotSize);
                using (SolidBrush green = new SolidBrush(Color.FromArgb(39, 201, 63)))
                    g.FillEllipse(green, termX + 60, dotY, dotSize, dotSize);

                // Window Title
                using (SolidBrush titleBrush = new SolidBrush(Color.FromArgb(145, 155, 180))) {
                    SizeF titleSize = g.MeasureString(title, titleFont);
                    float titleX = termX + (termW - titleSize.Width) / 2;
                    g.DrawString(title, titleFont, titleBrush, titleX, termY + 13);
                }

                // Render Content Lines
                float textY = termY + headerHeight + paddingY;
                foreach (string line in lines) {
                    Color textColor = Color.FromArgb(226, 232, 240); // default light slate
                    Font currentFont = font;

                    if (line.StartsWith("===") || line.StartsWith("---")) {
                        textColor = Color.FromArgb(97, 175, 239); // Vibrant Blue
                        currentFont = boldFont;
                    } else if (line.Contains("[Driver]") || line.Contains("successfully")) {
                        textColor = Color.FromArgb(152, 195, 121); // Sage Green
                        currentFont = boldFont;
                    } else if (line.Contains("[Passenger]")) {
                        textColor = Color.FromArgb(229, 192, 123); // Amber Yellow
                        currentFont = boldFont;
                    } else if (line.StartsWith("Enter your choice:")) {
                        textColor = Color.FromArgb(198, 120, 221); // Magenta
                        currentFont = boldFont;
                    } else if (line.Contains("Bus ID:") || line.Contains("Route ID:") || line.Contains("From:")) {
                        textColor = Color.FromArgb(86, 182, 194); // Cyan
                    } else if (line.Contains("UNIROUTE")) {
                        textColor = Color.FromArgb(255, 255, 255);
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

# 1. Startup & Main Menu
$l1 = @(
    "======================================================",
    "      UNIROUTE - COLLEGE BUS MANAGEMENT SYSTEM       ",
    "======================================================",
    "1. Display User Profiles (Driver & Passengers)",
    "2. Display Route & Waypoints",
    "3. Display Live Bus & Seat Status",
    "4. Board a Passenger (Simulate Heartbeat)",
    "5. Save Active Trip Log to File",
    "0. Exit System",
    "======================================================",
    "Enter your choice: _"
)
[CarbonStyler]::RenderTerminal("uniroute_core - main menu", $l1, "$projOut\fig_6_1_startup.png")

# 2. Option 1: User Profiles
$l2 = @(
    "Enter your choice: 1",
    "",
    "--- REGISTERED USERS ---",
    "[Driver] ID: 101 | Name: Ramesh Singh | Emp ID: EMP-DRV-01 | Assigned Bus: RJ-27-PA-1001",
    "[Passenger] ID: 201 | Name: Aarav Sharma | Roll No: 25EGICS124 | Stop: City Center",
    "[Passenger] ID: 202 | Name: Priya Verma | Roll No: 25EGICS144 | Stop: Subhash Nagar",
    "",
    "======================================================",
    "Enter your choice: _"
)
[CarbonStyler]::RenderTerminal("uniroute_core - option 1: user profiles", $l2, "$projOut\fig_6_2_user_profiles.png")

# 3. Option 2: Route Details
$l3 = @(
    "Enter your choice: 2",
    "",
    "--- ROUTE DETAILS ---",
    "",
    "Route ID: 1 | Title: Route 1: City Center to GITS Campus",
    "From: City Center ---> To: GITS Campus (4 Waypoint Stops)",
    "",
    "======================================================",
    "Enter your choice: _"
)
[CarbonStyler]::RenderTerminal("uniroute_core - option 2: route details", $l3, "$projOut\fig_6_3_route_details.png")

# 4. Option 3: Bus Capacity Status
$l4 = @(
    "Enter your choice: 3",
    "",
    "--- VEHICLE CAPACITY STATUS ---",
    "",
    "Bus ID: 1 | Number: RJ-27-PA-1001 | Capacity: 40 | On-Board: 0 | Available Seats: 40",
    "",
    "======================================================",
    "Enter your choice: _"
)
[CarbonStyler]::RenderTerminal("uniroute_core - option 3: bus capacity", $l4, "$projOut\fig_6_4_bus_status.png")

# 5. Option 4: Board Passenger
$l5 = @(
    "Enter your choice: 4",
    "",
    "Passenger boarded successfully.",
    "",
    "Bus ID: 1 | Number: RJ-27-PA-1001 | Capacity: 40 | On-Board: 1 | Available Seats: 39",
    "",
    "======================================================",
    "Enter your choice: _"
)
[CarbonStyler]::RenderTerminal("uniroute_core - option 4: board passenger", $l5, "$projOut\fig_6_5_board_passenger.png")

# 6. Option 5: Save Trip Log
$l6 = @(
    "Enter your choice: 5",
    "",
    "Trip details appended to trip_log.txt successfully.",
    "",
    "======================================================",
    "Enter your choice: _"
)
[CarbonStyler]::RenderTerminal("uniroute_core - option 5: file handling", $l6, "$projOut\fig_6_6_save_trip_log.png")

# 7. Notepad Trip Log Verification
$l7 = @(
    "trip_log.txt - Notepad",
    "File  Edit  View",
    "--------------------------------------------------------------------------------",
    "Route: Route 1: City Center to GITS Campus | From: City Center | To: GITS Campus | Status: Active",
    "--------------------------------------------------------------------------------"
)
[CarbonStyler]::RenderTerminal("trip_log.txt - text file storage", $l7, "$projOut\fig_6_7_trip_log_file.png")

# 8. Option 0: System Exit
$l8 = @(
    "Enter your choice: 0",
    "",
    "Thank you for using UniRoute System. Goodbye!",
    "",
    "Press Enter to exit console..."
)
[CarbonStyler]::RenderTerminal("uniroute_core - option 0: system exit", $l8, "$projOut\fig_6_8_system_exit.png")

# Copy all generated images to Desktop folder and Brain folder
Get-ChildItem -Path "$projOut\fig_6_*.png" | ForEach-Object {
    Copy-Item $_.FullName -Destination $desktopOut -Force
    Copy-Item $_.FullName -Destination $brainOut -Force
}

Get-ChildItem -Path $desktopOut
