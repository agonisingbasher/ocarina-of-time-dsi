param([string]$In, [string]$Out, [int]$X, [int]$Y, [int]$W, [int]$H, [int]$Scale = 6)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($In)
$bmp = New-Object System.Drawing.Bitmap ($W * $Scale), ($H * $Scale)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$dst = New-Object System.Drawing.Rectangle 0, 0, ($W * $Scale), ($H * $Scale)
$src = New-Object System.Drawing.Rectangle $X, $Y, $W, $H
$g.DrawImage($img, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $img.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
"saved $Out"
