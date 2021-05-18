#include "lasercontrol.h"

LaserControl::LaserControl(LaserCtrl *laser, QWidget *parent)
    : QWidget(parent)
    , ptr(laser)
{
    initDevice();
    initControlBox();
}

LaserControl::~LaserControl()
{
    stopDevice();
    delete groupWidget;
}

void LaserControl::initDevice()
{
    stopDevice();
    startDevice();
}

void LaserControl::initControlBox()
{
    groupWidget = new QGroupBox(ptr->name);

    voltageLabel = new QLabel(tr("Voltage:"));
    voltageInput = new QDoubleSpinBox;
    voltageInput->setDecimals(ptr->decimals);
    voltageInput->setSingleStep(ptr->stepsize);
    voltageInput->setRange(ptr->min, ptr->max);
    voltageInput->setSuffix(" V");
    voltageInput->setValue(ptr->value);
    QObject::connect(voltageInput, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, &LaserControl::changeVoltage);

    pLabel = new QLabel(tr("P(reg):"));
    pInput = new QLineEdit;
    pInput->setText(QString::number(ptr->p,'f',2));
    QObject::connect(pInput, &QLineEdit::editingFinished,
                     this, &LaserControl::changeP);
    iLabel = new QLabel(tr("I(reg):"));
    iInput = new QLineEdit;
    iInput->setText(QString::number(ptr->i,'f',2));
    QObject::connect(iInput, &QLineEdit::editingFinished,
                     this, &LaserControl::changeI);
    setpLabel = new QLabel(tr("Setpoint:"));
    setpInput = new QLineEdit;
    setpInput->setText(QString::number(ptr->setpoint,'f',5));
    QObject::connect(setpInput, &QLineEdit::editingFinished,
                     this, &LaserControl::changeSetP);
    merrLabel = new QLabel(tr("MaxError:"));
    merrInput = new QLineEdit;
    merrInput->setText(QString::number(ptr->maxerr,'f',5));
    QObject::connect(merrInput, &QLineEdit::editingFinished,
                     this, &LaserControl::changeMaxErr);

    regStatus = new QPushButton(tr("Lock State"));
    regStatus->setStyleSheet("QPushButton {background-color: white}");
    regSwitch = new QCheckBox(tr("Regulation"));
    regSwitch->setChecked(false);
    feedback_counter = 0;
    err_sum = 0.0;
    QObject::connect(regSwitch, &QCheckBox::clicked,
                     this, &LaserControl::changeOffset);

    controlBoxGrid = new QFormLayout;
    controlBoxGrid->addRow(voltageLabel, voltageInput);
    controlBoxGrid->addRow(setpLabel, setpInput);
    controlBoxGrid->addRow(merrLabel, merrInput);
    controlBoxGrid->addRow(pLabel, pInput);
    controlBoxGrid->addRow(iLabel, iInput);
    controlBoxGrid->addRow(regSwitch, regStatus);
    groupWidget->setLayout(controlBoxGrid);
}

void LaserControl::stopDevice()
{
    if (taskHandle) {
        DAQmxStopTask(taskHandle);
        DAQmxClearTask(taskHandle);
        taskHandle = 0;
    }
}

void LaserControl::startDevice()
{
    DAQmxCreateTask("", &taskHandle);
    qDebug() << QString("Analog output start with code %1.")
                .arg(DAQmxCreateAOVoltageChan(
                    taskHandle,
                    ptr->device.toUtf8(),
                    "",
                    ptr->min,
                    ptr->max,
                    DAQmx_Val_Volts,
                    NULL));
    DAQmxStartTask(taskHandle);
}

void LaserControl::changeVoltage()
{
    ptr->value = voltageInput->value();
    qDebug() << DAQmxWriteAnalogScalarF64(
                taskHandle,
                false,
                0,
                ptr->value,
                NULL);
}

void LaserControl::changeP()
{
    ptr->p = pInput->text().toDouble();
}

void LaserControl::changeI()
{
    ptr->i = iInput->text().toDouble();
}

void LaserControl::changeSetP()
{
    ptr->setpoint = setpInput->text().toDouble();
}

void LaserControl::changeMaxErr()
{
    ptr->maxerr = merrInput->text().toDouble();
}

void LaserControl::changeOffset()
{
    if (regSwitch->isChecked()) {
        if (abs(channels_freqs[ptr->wm_channel] - ptr->setpoint) > ptr->maxerr) { //out of range
            regSwitch->setChecked(false);
        } else offset = ptr->value;
    } else regStatus->setStyleSheet("QPushButton {background-color: white}");
}

void LaserControl::voltFeedback()
{
    qreal f_err = channels_freqs[ptr->wm_channel] - ptr->setpoint;
    if (!regSwitch->isChecked()) { // Regulation off
        return;
    }
    else {
        if (abs(f_err) < ptr->maxerr) { //counts valid data
            feedback_counter++;
            err_sum += f_err;
            regStatus->setStyleSheet("QPushButton {background-color: green}");
            regStatus->update();
        }
        else { //not locked or showing wrong freq, change lock state and clear counts
            regStatus->setStyleSheet("QPushButton {background-color: white}");
            regStatus->update();
            feedback_counter = 0;
            err_sum = 0.0;
        }
        // only do the feedback when the error is stable for given time
        if (feedback_counter >= ptr->fb_pending) {
            voltageInput->setValue( offset + qBound(ptr->fb_min,
                                    err_sum * ptr->i + f_err*ptr->p,
                                    ptr->fb_max) );
        }
    }
    return;
}
