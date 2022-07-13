Select Default Application
==========================

A very simple application that lets you define default applications on Linux in a sane way.

![screenshot](/screenshot.png)


How it works
------------

Basically it just loads all installed applications by reading their .desktop
files, reads the MimeType fields to see what it supports, and updates
~/.config/mimeapps.list with what the user wants.


Links
-----

 * https://specifications.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 * https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
 * https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html


Compiling
---------

    qmake my_project.pro
    make
