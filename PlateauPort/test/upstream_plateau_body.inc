void Plateau::process(const ProcessArgs &args) {
    getParameters();
    setLights();

    leftInput = clamp(leftInput, -10.f, 10.f);
    rightInput = clamp(rightInput, -10.f, 10.f);

    reverb.setTimeScale(size);
    reverb.setPreDelay(clamp(preDelay, 0.f, 1.f));
    reverb.setInputFilterLowCutoffPitch(inputDampLow);
    reverb.setInputFilterHighCutoffPitch(inputDampHigh);
    reverb.enableInputDiffusion(diffuseInput > 0.5f);
    reverb.setDecay(decay);
    reverb.setTankDiffusion(diffusion);
    reverb.setTankFilterLowCutFrequency(reverbDampLow);
    reverb.setTankFilterHighCutFrequency(reverbDampHigh);
    reverb.setTankModSpeed(modSpeed);
    reverb.setTankModDepth(modDepth);
    reverb.setTankModShape(modShape);

    reverb.process(leftInput * minus20dBGain * inputSensitivity * envelope._value,
                   rightInput * minus20dBGain * inputSensitivity * envelope._value);

    leftOutput = leftInput * dry + reverb.getLeftOutput() * wet *
                 envelope._value;
    rightOutput = rightInput * dry + reverb.getRightOutput() * wet *
                  envelope._value;

    if (softDriveOutput) {
        leftOutput = tanhDriveSignal(leftOutput * saturatorPreGain,
                                     saturatorDrive) * saturatorPostGain;
        rightOutput = tanhDriveSignal(rightOutput * saturatorPreGain,
                                      saturatorDrive) * saturatorPostGain;
        outputs[LEFT_OUTPUT].setVoltage(leftOutput);
        outputs[RIGHT_OUTPUT].setVoltage(rightOutput);
    }
    else {
        outputs[LEFT_OUTPUT].setVoltage(clamp(leftOutput, -10.f, 10.f));
        outputs[RIGHT_OUTPUT].setVoltage(clamp(rightOutput, -10.f, 10.f));
    }
}

void Plateau::getParameters() {
    freezeToggle = params[FREEZE_TOGGLE_PARAM].getValue() > 0.f;
    tuned = params[TUNED_MODE_PARAM].getValue() > 0.5f ? 1 : 0;
    diffuseInput = params[DIFFUSE_INPUT_PARAM].getValue();

    //Freeze
    if((params[FREEZE_PARAM].getValue() > 0.5f ||
        inputs[FREEZE_CV_INPUT].getVoltage() > 0.5f) && !freezeButtonState) {
        freeze = freezeToggle ? !freeze : true;
        freezeButtonState = true;
    }
    else if(params[FREEZE_PARAM].getValue() <= 0.5f 
            && inputs[FREEZE_CV_INPUT].getVoltage() <= 0.5f
            && freezeButtonState) {
        freeze = freezeToggle ? freeze : false;
        freezeButtonState = false;
    }

    if(freeze && !frozen) {
        frozen = true;
        reverb.freeze(frozen);
    }
    else if(!freeze && frozen){
        frozen = false;
        reverb.freeze(frozen);
    }

    // Clear
    if((params[CLEAR_PARAM].getValue() > 0.5f ||
        inputs[CLEAR_CV_INPUT].getVoltage() > 0.5f) && !clear && cleared) {
        cleared = false;
        clear = true;
    }
    else if((params[CLEAR_PARAM].getValue() <= 0.5f
             && inputs[CLEAR_CV_INPUT].getVoltage() <= 0.5f) && cleared) {
        clear = false;
    }

    if(clear) {
        if(!cleared && !fadeOut && !fadeIn) {
            fadeOut = true;
            envelope.setStartEndPoints(1.f, 0.f);
            envelope.trigger();
        }
        if(fadeOut && envelope._justFinished) {
            reverb.clear();
            fadeOut = false;
            fadeIn = true;
            envelope.setStartEndPoints(0.f, 1.f);
            envelope.trigger();
        }
        if(fadeIn && envelope._justFinished) {
            fadeIn = false;
            cleared = true;
            envelope._value = 1.f;
        }
    }
    envelope.process();

    // CV
    switch(preDelayCVSensState) {
        case 0: preDelayCVSens = preDelayNormSens; break;
        case 1: preDelayCVSens = preDelayLowSens;
    }
    preDelay = params[PRE_DELAY_PARAM].getValue();
    preDelay += 0.5f * (powf(2.f,inputs[PRE_DELAY_CV_INPUT].getVoltage() *
                             preDelayCVSens) - 1.f);

    size = inputs[SIZE_CV_INPUT].getVoltage() *
           params[SIZE_CV_PARAM].getValue() * 0.1f;
    size += params[SIZE_PARAM].getValue();

    if(tuned) {
        size = sizeMin * powf(2.f, size * 5.f);
        size = clamp(size, sizeMin, 2.5f);
    }
    else {
        size *= size;
        size = rescale(size, 0.f, 1.f, 0.01f, sizeMax);
        size = clamp(size, 0.01f, sizeMax);
    }

    diffusion = inputs[DIFFUSION_CV_INPUT].getVoltage() *
                params[DIFFUSION_CV_PARAM].getValue();
    diffusion += params[DIFFUSION_PARAM].getValue();
    diffusion = clamp(diffusion, 0.f, 10.f);


    decay = rescale(inputs[DECAY_CV_INPUT].getVoltage() *
                    params[DECAY_CV_PARAM].getValue(), 0.f, 10.f, 0.1f, 0.999f);
    decay += params[DECAY_PARAM].getValue();
    decay = clamp(decay, 0.1f, decayMax);
    decay = 1.f - decay;
    decay = 1.f - decay * decay;

    inputDampLow = inputs[INPUT_LOW_DAMP_CV_INPUT].getVoltage() *
                   params[INPUT_LOW_DAMP_CV_PARAM].getValue();
    inputDampLow += (params[INPUT_LOW_DAMP_PARAM].getValue() + 5.f);
    inputDampLow = clamp(inputDampLow, 0.f, 10.f);
    inputDampLow = 10.f - inputDampLow;

    inputDampHigh = inputs[INPUT_HIGH_DAMP_CV_INPUT].getVoltage() *
                    params[INPUT_HIGH_DAMP_CV_PARAM].getValue();
    inputDampHigh += (params[INPUT_HIGH_DAMP_PARAM].getValue() + 5.f);
    inputDampHigh = clamp(inputDampHigh, 0.f, 10.f);

    reverbDampLow = inputs[REVERB_LOW_DAMP_CV_INPUT].getVoltage() *
                    params[REVERB_LOW_DAMP_CV_PARAM].getValue();
    reverbDampLow += (params[REVERB_LOW_DAMP_PARAM].getValue() + 5.f);
    reverbDampLow = clamp(reverbDampLow, 0.f, 10.f);
    reverbDampLow = 10.f - reverbDampLow;

    reverbDampHigh = inputs[REVERB_HIGH_DAMP_CV_INPUT].getVoltage() *
                     params[REVERB_HIGH_DAMP_CV_PARAM].getValue();
    reverbDampHigh += (params[REVERB_HIGH_DAMP_PARAM].getValue() + 5.f);
    reverbDampHigh = clamp(reverbDampHigh, 0.f, 10.f);

    modSpeed = inputs[MOD_SPEED_CV_INPUT].getVoltage() *
               params[MOD_SPEED_CV_PARAM].getValue() * 0.1f;
    modSpeed += params[MOD_SPEED_PARAM].getValue();
    modSpeed = clamp(modSpeed, modSpeedMin, modSpeedMax);
    modSpeed *= modSpeed;
    modSpeed = modSpeed * 99.f + 1.f;

    modShape = inputs[MOD_SHAPE_CV_INPUT].getVoltage() *
               params[MOD_SHAPE_CV_PARAM].getValue() * 0.1f;
    modShape += params[MOD_SHAPE_PARAM].getValue();
    modShape = rescale(modShape, 0.f, 1.f, modShapeMin, modShapeMax);
    modShape = clamp(modShape, modShapeMin, modShapeMax);

    modDepth = inputs[MOD_DEPTH_CV_INPUT].getVoltage() *
               params[MOD_DEPTH_CV_PARAM].getValue();
    modDepth = rescale(modDepth, 0.f, 10.f, modDepthMin, modDepthMax);
    modDepth += params[MOD_DEPTH_PARAM].getValue();
    modDepth = clamp(modDepth, modDepthMin, modDepthMax);

    leftInput = inputs[LEFT_INPUT].getVoltageSum();
    rightInput = inputs[RIGHT_INPUT].getVoltageSum();

    if(!inputs[LEFT_INPUT].isConnected() &&
        inputs[RIGHT_INPUT].isConnected()) {
        leftInput = rightInput;
    }
    else if(inputs[LEFT_INPUT].isConnected() &&
            !inputs[RIGHT_INPUT].isConnected()) {
        rightInput = leftInput;
    }

    inputSensitivity = inputSensitivityState ? minus18dBGain : zerodBGain;

    dry = inputs[DRY_CV_INPUT].getVoltage() * params[DRY_CV_PARAM].getValue();
    dry += params[DRY_PARAM].getValue();
    dry = clamp(dry, 0.f, 1.f);

    wet = inputs[WET_CV_INPUT].getVoltage() * params[WET_CV_PARAM].getValue();
    wet += params[WET_PARAM].getValue();
    wet = clamp(wet, 0.f, 1.f) * 10.f;
}

void Plateau::setLights() {
    lights[FREEZE_TOGGLE_LIGHT].value = freezeToggle ? 10.f : 0.f;
    lights[FREEZE_LIGHT].value = freeze ? 10.f : 0.f;
    lights[TUNED_MODE_LIGHT].value = tuned ? 10.f : 0.f;
    lights[DIFFUSE_INPUT_LIGHT].value = diffuseInput > 0.5f ? 10.f : 0.f;
    lights[CLEAR_LIGHT].value = clear ? 10.f : 0.f;
}

