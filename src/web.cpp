#include "web.h"

// /
void Web::_s0(void)
{
    String html;
    html.reserve(3072);
    html += F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
              "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
              "<title>sd_pulse_logger</title><style>"
              "body{font-family:sans-serif;margin:1em;max-width:40em}"
              "table{border-collapse:collapse;width:100%}"
              "td,th{padding:.4em;border-bottom:1px solid #ccc;text-align:left}"
              ".cur{font-weight:bold}"
              "</style></head><body><h2>sd_pulse_logger</h2>");
    if (!_sdw->mounted())
    {
        html += F("<p>SD card is not mounted.</p></body></html>");
        _server.send(503, "text/html", html);
        return;
    }
    const String current = _sdw->filename();
    if (current.length() > 0)
    {
        html += F("<p>Current file: <b>");
        html += current.substring(1);
        html += F("</b> [<a href=\"/view\">view</a>] [<a href=\"/download\">download</a>]</p>");
    }
    else
    {
        html += F("<p>No file.</p>");
    }
    html += F("<table><tr><th>File</th><th>Size</th><th></th></tr>");
    File root = SD.open("/");
    if (root && root.isDirectory())
    {
        for (File f = root.openNextFile(); f; f = root.openNextFile())
        {
            String name = f.name();
            if (!name.startsWith("/"))
                name = String("/") + name;
            if (!f.isDirectory())
            {
                const bool curr = (name == current);
                html += curr ? F("<tr class=\"cur\"><td>") : F("<tr><td>");
                html += name.substring(1);
                html += F("</td><td>");
                html += f.size();
                html += F("</td><td><a href=\"/view?name=");
                html += name;
                html += F("\">view</a> <a href=\"/download?name=");
                html += name;
                html += F("\">download</a>");
                if (!curr)
                {
                    html += F(" <a href=\"#\" onclick=\"return d('");
                    html += name;
                    html += F("')\">delete</a>");
                }
                html += F("</td></tr>");
            }
            f.close();
        }
    }
    if (root)
        root.close();
    html += F("</table>"
              "<form id=\"del\" method=\"post\" action=\"/delete\">"
              "<input type=\"hidden\" name=\"name\" id=\"dn\"></form>"
              "<script>function d(n){"
              "if(confirm('Delete '+n.substring(1)+'?')){"
              "document.getElementById('dn').value=n;"
              "document.getElementById('del').submit();}"
              "return false;}</script>"
              "</body></html>");
    _server.send(200, "text/html", html);
}

// /download
void Web::_s1(bool mode)
{
    if (!_sdw->mounted())
    {
        _server.send(503, "text/plain", "SD card is not mounted");
        return;
    }
    String name = _server.hasArg("name") ? _server.arg("name") : _sdw->filename();
    if (name.length() == 0)
    {
        _server.send(404, "text/plain", "No log file is open");
        return;
    }
    File f = SD.open(name, FILE_READ);
    if (!f || f.isDirectory())
    {
        if (f)
            f.close();
        _server.send(404, "text/plain", "File not found");
        return;
    }
    if (mode)
        _server.sendHeader("Content-Disposition",
                           String("attachment; filename=\"") + name.substring(1) + "\"");
    _server.streamFile(f, mode ? "text/csv" : "text/plain; charset=utf-8");
    f.close();
}

// /delete
void Web::_s2()
{
    if (!_sdw->mounted())
    {
        _server.send(503, "text/plain", "SD card is not mounted");
        return;
    }
    if (!_server.hasArg("name"))
    {
        _server.send(400, "text/plain", "Missing name");
        return;
    }
    String name = _server.arg("name");
    if (!name.startsWith("/"))
        name = String("/") + name;
    if (name.length() < 2 || name.indexOf('/', 1) >= 0 || name.indexOf("..") >= 0)
    {
        _server.send(400, "text/plain", "Invalid file name");
        return;
    }
    if (name == _sdw->filename())
    {
        _server.send(409, "text/plain", "Cannot delete the current log file");
        return;
    }
    File f = SD.open(name, FILE_READ);
    if (!f || f.isDirectory())
    {
        if (f)
            f.close();
        _server.send(404, "text/plain", "File not found");
        return;
    }
    f.close();
    if (!SD.remove(name))
    {
        _server.send(500, "text/plain", "Delete failed");
        return;
    }
    _server.sendHeader("Location", "/");
    _server.send(303, "text/plain", "Deleted");
}