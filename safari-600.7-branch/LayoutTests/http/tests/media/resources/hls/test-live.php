<?php
# See the HLS ITEF spec: <http://tools.ietf.org/id/draft-pantos-http-live-streaming-12.txt>
header("Status: 200 OK");
header("HTTP/1.1 200 OK");
header("Connection: close");
header("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT");
header("Pragma: no-cache");
header("Etag: " . '"' . filesize(__FILE__) . "-" . filemtime(__FILE__) . '"');
header("Content-Type: application/x-mpegurl");
header("Content-Length: " . filesize(__FILE__));

$chunkDuration = 6.0272;
$chunkCount = 4;

function println($string) { return print($string . PHP_EOL); }
println("#EXTM3U");
println("#EXT-X-TARGETDURATION:7");
println("#EXT-X-VERSION:4");
println("#EXT-X-MEDIA-SEQUENCE:" . (time() / $chunkDuration % 100));

$time = time();
$time = $time - $time % $chunkDuration;

for ($i = 0; $i < $chunkCount; ++$i) {
	$time += 6;
	println("#EXT-X-PROGRAM-DATE-TIME:" . gmdate("c", $time));
	println("#EXTINF:" . $chunkDuration . ",");
	println("test.ts");
}
