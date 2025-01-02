/*
 *  pluginbaseaudio.cpp  -  base class for plugin to provide an audio backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "pluginbaseaudio.h"

PluginBaseAudio::PluginBaseAudio(QObject* parent, const QList<QVariant>& args)
    : QObject(parent)
{
    Q_UNUSED(args)
}

PluginBaseAudio::~PluginBaseAudio() = default;

#include "moc_pluginbaseaudio.cpp"

// vim: et sw=4:
