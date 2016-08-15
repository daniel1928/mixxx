#include <iostream>
#include <dlfcn.h>
#include <QDebug>

#include "layoutsfilehandler.h"

LayoutsFileHandler::LayoutsFileHandler() {}

LayoutsFileHandler::~LayoutsFileHandler() {}

void LayoutsFileHandler::open(QString &cppPath, QList<Layout> &layouts) {
    QFileInfo check_file(cppPath);
    if (!check_file.exists() || !check_file.isFile()) {
        cppPath = "";
        qDebug() << "Not loading layouts. Path doesn't exist: " << cppPath;
        return;
    }

    QFile f(cppPath);
    LayoutNamesData layoutNames = getLayoutNames(f);

    // Add some code in order for this tool to compile
    prependDefs(f);
    appendGetLayoutsFunction(f, layoutNames);

    // Compile the file and get the function pointer to the getLayout
    // function (and the handle to be able to close it when we are done)
    GetLayout_t getLayout = nullptr;
    void *handle = nullptr;
    compileLayoutsFile(cppPath,
                       getLayout,
                       handle);

    for (QStringList &names : layoutNames) {
        QString &varName = names[0];
        QString &name = names[1];
        KeyboardLayoutPointer layoutData = getLayout(varName.toLatin1().data());

        // Construct layout object and append to layouts
        Layout layout(varName, name, layoutData);
        layouts.append(layout);
    }

    // Close layouts library
    qDebug() << "Closing layouts library...\n";
    dlclose(handle);

    // Write a clean version of the loaded layouts (remove definitions
    // noise added in prependDefs)
    save(f, layouts);
}

void LayoutsFileHandler::compileLayoutsFile(const QString cppPath, GetLayout_t &pGetLayoutFn, void *&handle) {
    const QDir dir = QFileInfo(cppPath).absoluteDir();
    const QString soPath = dir.filePath("layouts.so");

    // Compile layouts
    system(("gcc -std=c++11 -shared -o " + soPath + " -fPIC " + cppPath).toLatin1().data());

    // Open layouts
    qDebug() << "Opening " << soPath;
    handle = dlopen(soPath.toLatin1().data(), RTLD_LAZY);
    if (!handle) {
        qFatal("Could not open layouts library");
    }

    // Load getLayout function
    qDebug() << "Loading getLayout function symbol...";
    pGetLayoutFn = (GetLayout_t) dlsym(handle, "getLayout");
    if (!pGetLayoutFn) {
        qCritical() << dlerror();
        dlclose(handle);
        qFatal("Couldn't load symbol 'getLayout'");
    }
}

void LayoutsFileHandler::prependDefs(QFile &cppFile) {
    QStringList lines;

    // Include iostream
    lines.append("#include <iostream>");

    // Add KbdKeyChar struct definition
    lines.append("struct KbdKeyChar {");
    lines.append("    char16_t character;");
    lines.append("    bool is_dead;");
    lines.append("};");

    // Add KeyboardLayoutPointer definition
    lines.append("typedef const KbdKeyChar (*KeyboardLayoutPointer)[2];");

    // Load each line of file into QStringList
    QStringList fileLines;
    if (cppFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&cppFile);
        while (!in.atEnd()) {
            fileLines.append(in.readLine());
        }
        cppFile.close();
    }

    lines += fileLines;

    // Overwrite file with prepended definitions
    if (cppFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        QTextStream stream(&cppFile);
        for (QStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
            stream << *it << "\n";
        }
        cppFile.close();
    }
}

void LayoutsFileHandler::appendGetLayoutsFunction(QFile &cppFile, const LayoutNamesData &layoutNames) {
    QString beginLayoutComment = "/* @BEGIN_GET_LAYOUT */";
    QString endLayoutComment = "/* @END_GET_LAYOUT */";

    // Remove old 'getLayout' function definition(s)
    QStringList fileLines;
    if (cppFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&cppFile);
        bool skip = false;
        while (!in.atEnd()) {
            QString line = in.readLine();
            QString trimmedLine = line.trimmed();

            // If we are within beginLayoutComment and endLayoutComment,
            // do not add those lines in
            if (trimmedLine.startsWith(beginLayoutComment)) skip = true;
            if (!skip) fileLines.append(line);
            if (trimmedLine.startsWith(endLayoutComment)) skip = false;
        }
        cppFile.close();
    }

    // Generate new 'getLayout' function definition
    QStringList fnLines;
    QString indent = "    ";
    fnLines.append("");
    fnLines.append(beginLayoutComment);
    fnLines.append("extern \"C\" KeyboardLayoutPointer getLayout(std::string layoutName) {");

    // Create one if-statement per layout
    for (QStringList names : layoutNames)
        fnLines.append(
                QString("%1if (layoutName == \"%2\") return layouts::%2;").arg(indent, names[0])
        );

    // Create 'else' if there is an 'if', if no 'if', return nullptr directly
    if (!layoutNames.isEmpty()) {
        fnLines.append(QString("%1else {").arg(indent));
        fnLines.append(QString("%1%1return nullptr;").arg(indent));
        fnLines.append(QString("%1}").arg(indent));
    } else {
        fnLines.append(QString("%1// There are no layouts in this file, so I can't return any").arg(indent));
        fnLines.append(QString("%1return nullptr;").arg(indent));
    }
    fnLines.append("}");
    fnLines.append(endLayoutComment);

    // Append new definition to buffered file
    fileLines += fnLines;

    // Rewrite file from buffer
    if (cppFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        QTextStream stream(&cppFile);
        for (QStringList::Iterator it = fileLines.begin(); it != fileLines.end(); ++it) {
            stream << *it << "\n";
        }
        cppFile.close();
    }
}

LayoutNamesData LayoutsFileHandler::getLayoutNames(QFile &cppFile) {
    LayoutNamesData names;

    if (cppFile.open(QIODevice::ReadOnly)) {
        QRegExp bracketRegex("\\[");
        QString type = "static const KbdKeyChar";
        QTextStream in(&cppFile);

        QString prevLine;
        while (!in.atEnd()) {

            // Get current line and trim to get rid of indentation
            QString line = in.readLine().trimmed();

            if (!line.startsWith(type)) {
                prevLine = line;
                continue;
            }

            // Retrieve variable name by chopping line
            line = line.mid(24);
            line = line.mid(0, bracketRegex.indexIn(line));

            QString varName = line;
            QString name = prevLine.startsWith("//") ? prevLine.mid(3) : "";

            // Create QStringList and append it to names
            QStringList currentLayoutNames;
            currentLayoutNames.append(varName);
            currentLayoutNames.append(name);
            names.append(currentLayoutNames);

            prevLine = line;
        }

        cppFile.close();
    }

    return names;
}

void LayoutsFileHandler::save(QFile &f, QList<Layout> &layouts) {
    QStringList lines;

    // Open namespace
    lines.append("namespace layouts {");
    lines.append("");

    // Add layouts and add indentation for namespace
    for (Layout &layout : layouts) {
        QStringList layoutCode = layout.generateCode();
        for (QString &line : layoutCode) {
            lines.append("    " + line);
        }

        lines.append("");
    }

    // Close namespace
    lines.append("}");

    if (f.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        QTextStream stream(&f);
        for (QStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
            stream << *it << "\n";
        }
        f.close();
    }

    appendGetLayoutsFunction(f, getLayoutNames(f));
}