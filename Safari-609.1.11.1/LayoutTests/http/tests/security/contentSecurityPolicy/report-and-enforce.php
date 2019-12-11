<?php
    header("Content-Security-Policy-Report-Only: script-src 'self'; report-uri resources/save-report.php");
?>
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="Content-Security-Policy" content="img-src 'none'">
</head>
<body>
    This image should be blocked, but should not show up in the violation report.
    <img src="../resources/abe.png">
    <script>
        // This script block will trigger a violation report but shouldn't be blocked.
        alert('PASS');
    </script>
    <script src="resources/go-to-echo-report.js"></script>
</body>
</html>
