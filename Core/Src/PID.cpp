#include "PID.h"

/**
 * Constructs the PIDController object with PID Gains and function pointers
 * for retrieving feedback (pidSource) and delivering output (pidOutput).
 * All PID gains should be positive, otherwise the system will violently diverge
 * from the target.
 * @param p The Proportional gain.
 * @param i The Integral gain.
 * @param d The Derivative gain.
 * @param (*pidSource) The function pointer for retrieving system feedback.
 * @param (*pidOutput) The function pointer for delivering system output.
 */
template <class T>
PIDController<T>::PIDController(double p, double i, double d, T (*pidSource)(), void (*pidOutput)(T output))
{
  _p = p;
  _i = i;
  _d = d;
  target = 0;
  output = 0;
  enabled = true;
  currentFeedback = 0;
  lastFeedback = 0;
  error = 0;
  lastError = 0;
  currentTime = 0L;
  lastTime = 0L;
  integralCumulation = 0;
  maxCumulation = 30000;
  cycleDerivative = 0;

  inputBounded = false;
  inputLowerBound = 0;
  inputUpperBound = 0;
  outputBounded = false;
  outputLowerBound = 0;
  outputUpperBound = 0;
  feedbackWrapped = false;

  timeFunctionRegistered = false;
  _pidSource = pidSource;
  _pidOutput = pidOutput;
}

/**
 * This method uses the established function pointers to retrieve system
 * feedback, calculate the PID output, and deliver the correction value
 * to the parent of this PIDController.  This method should be run as
 * fast as the source of the feedback in order to provide the highest
 * resolution of control (for example, to be placed in the loop() method).
 */
template <class T>
void PIDController<T>::tick()
{
  if(enabled)
  {
    //Retrieve system feedback from user callback.
    currentFeedback = _pidSource();

    //Apply input bounds if necessary.
    if(inputBounded)
    {
      if(currentFeedback > inputUpperBound) currentFeedback = inputUpperBound;
      if(currentFeedback < inputLowerBound) currentFeedback = inputLowerBound;
    }

    /*
     * Feedback wrapping causes two distant numbers to appear adjacent to one
     * another for the purpose of calculating the system's error.
     */
    if(feedbackWrapped)
    {
      /*
       * There are three ways to traverse from one point to another in this setup.
       *
       *    1)  Target --> Feedback
       *
       * The other two ways involve bridging a gap connected by the upper and
       * lower bounds of the feedback wrap.
       *
       *    2)  Target --> Upper Bound == Lower Bound --> Feedback
       *
       *    3)  Target --> Lower Bound == Upper Bound --> Feedback
       *
       * Of these three paths, one should always be shorter than the other two,
       * unless all three are equal, in which case it does not matter which path
       * is taken.
       */
      int regErr = target - currentFeedback;
      int altErr1 = (target - feedbackWrapLowerBound) + (feedbackWrapUpperBound - currentFeedback);
      int altErr2 = (feedbackWrapUpperBound - target) + (currentFeedback - feedbackWrapLowerBound);

      //Calculate the absolute values of each error.
      int regErrAbs = (regErr >= 0) ? regErr : -regErr;
      int altErr1Abs = (altErr1 >= 0) ? altErr1 : -altErr1;
      int altErr2Abs = (altErr2 >= 0) ? altErr2 : -altErr2;

      //Use the error with the smallest absolute value
      if(regErrAbs <= altErr1Abs && regErr <= altErr2Abs) //If reguErrAbs is smallest
      {
        error = regErr;
      }
      else if(altErr1Abs < regErrAbs && altErr1Abs < altErr2Abs) //If altErr1Abs is smallest
      {
        error = altErr1Abs;
      }
      else if(altErr2Abs < regErrAbs && altErr2Abs < altErr1Abs) //If altErr2Abs is smallest
      {
        error = altErr2Abs;
      }
    }
    else
    {
      //Calculate the error between the feedback and the target.
      error = target - currentFeedback;
    }

    //If we have a registered way to retrieve the system time, use time in PID calculations.
    if(timeFunctionRegistered)
    {
      //Retrieve system time
      currentTime = _getSystemTime();

      //Calculate time since last tick() cycle.
      long deltaTime = currentTime - lastTime;

      //Calculate the integral of the feedback data since last cycle.
      int cycleIntegral = (lastError + error / 2) * deltaTime;

      //Add this cycle's integral to the integral cumulation.
      integralCumulation += cycleIntegral;

      //Calculate the slope of the line with data from the current and last cycles.
      cycleDerivative = (error - lastError) / deltaTime;

      //Save time data for next iteration.
      lastTime = currentTime;
    }
    //If we have no way to retrieve system time, estimate calculations.
    else
    {
      integralCumulation += error;
      cycleDerivative = (error - lastError);
    }

    //Prevent the integral cumulation from becoming overwhelmingly huge.
    if(integralCumulation > maxCumulation) integralCumulation = maxCumulation;
    if(integralCumulation < -maxCumulation) integralCumulation = -maxCumulation;

    //Calculate the system output based on data and PID gains.
    output = (int) ((error * _p) + (integralCumulation * _i) + (cycleDerivative * _d));

    //Save a record of this iteration's data.
    lastFeedback = currentFeedback;
    lastError = error;

    //Trim the output to the bounds if needed.
    if(outputBounded)
    {
      if(output > outputUpperBound) output = outputUpperBound;
      if(output < outputLowerBound) output = outputLowerBound;
    }

    _pidOutput(output);
  }
}

/**
 * Sets the target of this PIDController.  This system will generate
 * correction outputs indended to guide the feedback variable (such
 * as position, velocity, etc.) toward the established target.
 */
template <class T>
void PIDController<T>::setTarget(T t)
{
  target = t;
}

/**
 * Returns the current target of this PIDController.
 * @return The current target of this PIDController.
 */
template <class T>
T PIDController<T>::getTarget()
{
  return target;
}

/**
 * Returns the latest output generated by this PIDController.  This value is
 * also delivered to the parent systems via the PIDOutput function pointer
 * provided in the constructor of this PIDController.
 * @return The latest output generated by this PIDController.
 */
template <class T>
T PIDController<T>::getOutput()
{
  return output;
}

/**
 * Returns the last read feedback of this PIDController.
 * @return The
 */
template <class T>
T PIDController<T>::getFeedback()
{
  return currentFeedback;
}

/**
 * Returns the last calculated error of this PIDController.
 * @return The last calculated error of this PIDController.
 */
template <class T>
T PIDController<T>::getError()
{
  return error;
}

/**
 * Enables or disables this PIDController.
 * @param True to enable, False to disable.
 */
template <class T>
void PIDController<T>::setEnabled(bool e)
{
  //If the PIDController was enabled and is being disabled.
  if(!e && enabled)
  {
    output = 0;
    integralCumulation = 0;
  }
  enabled = e;
}

/**
 * Tells whether this PIDController is enabled.
 * @return True for enabled, false for disabled.
 */
template <class T>
bool PIDController<T>::isEnabled()
{
  return enabled;
}

/**
 * Returns the value that the Proportional component is contributing to the output.
 * @return The value that the Proportional component is contributing to the output.
 */
template <class T>
T PIDController<T>::getProportionalComponent()
{
  return (T) (error * _p);
}

/**
 * Returns the value that the Integral component is contributing to the output.
 * @return The value that the Integral component is contributing to the output.
 */
template <class T>
T PIDController<T>::getIntegralComponent()
{
  return (T) (integralCumulation * _i);
}

/**
 * Returns the value that the Derivative component is contributing to the output.
 * @return The value that the Derivative component is contributing to the output.
 */
template <class T>
T PIDController<T>::getDerivativeComponent()
{
  return (T) (cycleDerivative * _d);
}

/**
 * Sets the maximum value that the integral cumulation can reach.
 * @param max The maximum value of the integral cumulation.
 */
template <class T>
void PIDController<T>::setMaxIntegralCumulation(T max)
{
  //If the new max value is less than 0, invert to make positive.
  if(max < 0)
  {
    max = -max;
  }

  //If the new max is not more than 1 then the cumulation is useless.
  if(max > 1)
  {
    maxCumulation = max;
  }
}

/**
 * Returns the maximum value that the integral value can cumulate to.
 * @return The maximum value that the integral value can cumulate to.
 */
template <class T>
T PIDController<T>::getMaxIntegralCumulation()
{
  return maxCumulation;
}

/**
 * Returns the current cumulative integral value in this PIDController.
 * @return The current cumulative integral value in this PIDController.
 */
template <class T>
T PIDController<T>::getIntegralCumulation()
{
  return integralCumulation;
}

/**
 * Enables or disables bounds on the input.  Bounds limit the upper and
 * lower values that this PIDController will ever accept as input.
 * Outlying values will be trimmed to the upper or lower bound as necessary.
 * @param bounded True to enable input bounds, False to disable.
 */
template <class T>
void PIDController<T>::setInputBounded(bool bounded)
{
  inputBounded = bounded;
}

/**
 * Returns whether the input of this PIDController is being bounded.
 * @return True if the input of this PIDController is being bounded.
 */
template <class T>
bool PIDController<T>::isInputBounded()
{
  return inputBounded;
}

/**
 * Sets bounds which limit the lower and upper extremes that this PIDController
 * accepts as inputs.  Outliers are trimmed to the lower and upper bounds.
 * Setting input bounds automatically enables input bounds.
 * @param lower The lower input bound.
 * @param upper The upper input bound.
 */
template <class T>
void PIDController<T>::setInputBounds(T lower, T upper)
{
  if(upper > lower)
  {
    inputBounded = true;
    inputUpperBound = upper;
    inputLowerBound = lower;
  }
}

/**
 * Returns the lower input bound of this PIDController.
 * @return The lower input bound of this PIDController.
 */
template <class T>
T PIDController<T>::getInputLowerBound()
{
  return inputLowerBound;
}

/**
 * Returns the upper input bound of this PIDController.
 * @return The upper input bound of this PIDController.
 */
template <class T>
T PIDController<T>::getInputUpperBound()
{
  return inputUpperBound;
}

/**
 * Enables or disables bounds on the output.  Bounds limit the upper and lower
 * values that this PIDController will ever generate as output.
 * @param bounded True to enable output bounds, False to disable.
 */
template <class T>
void PIDController<T>::setOutputBounded(bool bounded)
{
  outputBounded = bounded;
}

/**
 * Returns whether the output of this PIDController is being bounded.
 * @return True if the output of this PIDController is being bounded.
 */
template <class T>
bool PIDController<T>::isOutputBounded()
{
  return outputBounded;
}

/**
 * Sets bounds which limit the lower and upper extremes that this PIDController
 * will ever generate as output.  Setting output bounds automatically enables
 * output bounds.
 * @param lower The lower output bound.
 * @param upper The upper output bound.
 */
template <class T>
void PIDController<T>::setOutputBounds(T lower, T upper)
{
  if(upper > lower)
  {
    outputBounded = true;
    outputLowerBound = lower;
    outputUpperBound = upper;
  }
}

/**
 * Returns the lower output bound of this PIDController.
 * @return The lower output bound of this PIDController.
 */
template <class T>
T PIDController<T>::getOutputLowerBound()
{
  return outputLowerBound;
}

/**
 * Returns the upper output bound of this PIDController.
 * @return The upper output bound of this PIDController.
 */
template <class T>
T PIDController<T>::getOutputUpperBound()
{
  return outputUpperBound;
}

/**
 * Enables or disables feedback wrapping.
 * Feedback wrapping causes the upper and lower bounds to appear adjacent to
 * one another when calculating system error.  This can be useful for rotating
 * systems which use degrees as units.  For example, wrapping the bounds
 * [0, 360] will cause a target of 5 and a feedback of 355 to produce an error
 * of -10 rather than 350.
 * @param wrapped True to enable feedback wrapping, False to disable.
 */
template <class T>
void PIDController<T>::setFeedbackWrapped(bool wrapped)
{
  feedbackWrapped = wrapped;
}

/**
 * Returns whether this PIDController has feedback wrap.
 * @return Whether this PIDController has feedback wrap.
 */
template <class T>
bool PIDController<T>::isFeedbackWrapped()
{
  return feedbackWrapped;
}

/**
 * Sets the bounds which the feedback wraps around. This
 * also enables Input Bounds at the same coordinates to
 * prevent extraneous domain errors.
 * @param lower The lower wrap bound.
 * @param upper The upper wrap bound.
 */
template <class T>
void PIDController<T>::setFeedbackWrapBounds(T lower, T upper)
{
  //Make sure no value outside this circular range is ever input.
  setInputBounds(lower, upper);

  feedbackWrapped = true;
  feedbackWrapLowerBound = lower;
  feedbackWrapUpperBound = upper;
}

/**
 * Returns the lower feedback wrap bound.
 * @return The lower feedback wrap bound.
 */
template <class T>
T PIDController<T>::getFeedbackWrapLowerBound()
{
  return feedbackWrapLowerBound;
}

/**
 * Returns the upper feedback wrap bound.
 * @return The upper feedback wrap bound.
 */
template <class T>
T PIDController<T>::getFeedbackWrapUpperBound()
{
  return feedbackWrapUpperBound;
}

/**
 * Sets new values for all PID Gains.
 * @param p The new proportional gain.
 * @param i The new integral gain.
 * @param d The new derivative gain.
 */
template <class T>
void PIDController<T>::setPID(double p, double i, double d)
{
  _p = p;
  _i = i;
  _d = d;
}

/**
 * Sets a new value for the proportional gain.
 * @param p The new proportional gain.
 */
template <class T>
void PIDController<T>::setP(double p)
{
  _p = p;
}

/**
 * Sets a new value for the integral gain.
 * @param i The new integral gain.
 */
template <class T>
void PIDController<T>::setI(double i)
{
  _i = i;
}

/**
 * Sets a new value for the derivative gain.
 * @param d The new derivative gain.
 */
template <class T>
void PIDController<T>::setD(double d)
{
  _d = d;
}

/**
 * Returns the proportional gain.
 * @return The proportional gain.
 */
template <class T>
double PIDController<T>::getP()
{
  return _p;
}

/**
 * Returns the integral gain.
 * @return The integral gain.
 */
template <class T>
double PIDController<T>::getI()
{
  return _i;
}

/**
 * Returns the derivative gain.
 * @return The derivative gain.
 */
template <class T>
double PIDController<T>::getD()
{
  return _d;
}

/**
 * Sets the function pointer to the PID Source.  A PID Source
 * is a function which returns a value to be used as the PIDController's
 * control feedback.  This value can be a reading from a sensor or other
 * data source that contains information regarding the system's actual
 * state.
 * Below is an example of using a PIDSource:
 *
 *    int pidSource()
 *    {
 *        return mySensor.getValue();
 *    }
 *    myPIDController.setPIDSource(pidSource);
 *
 * @param (*getFeedback) A function pointer that retrieves system feedback.
 */
template <class T>
void PIDController<T>::setPIDSource(T (*pidSource)())
{
  _pidSource = pidSource;
}

/**
 * Sets the function pointer to the PID Output.  A PID Output
 * is a function which delivers a value to the parent system in order to guide
 * the system based on the PID loop's result.  This value can be delivered
 * directly to motors, to a variable that directs steering, or other means of
 * influencing the system.
 * Below is an example of using a PIDOutput:
 *
 *    void pidOutput(int output)
 *    {
 *        myMotor.write(output);
 *    }
 *    myPIDController.setPIDOutput(pidOutput);
 *
 * @param (*onUpdate) A function pointer that delivers system output.
 */
template <class T>
void PIDController<T>::setPIDOutput(void (*pidOutput)(T output))
{
  _pidOutput = pidOutput;
}

/**
 * Use this to add a hook into the PID Controller that allows it to
 * read the system time no matter what platform this library is run
 * on.  Though developed for an arduino project, there is no code
 * tying this library to the arduino project.  To use the arduino
 * clock, however, register the time-getting function (millis())
 * like this:
 *
 *    myPIDController.registerTimeInput(millis);
 *    *Note that in this example, millis has no parentheses.
 *
 * @param (*getSystemTime) Pointer to a function that returns system time.
 */
template <class T>
void PIDController<T>::registerTimeFunction(unsigned long (*getSystemTime)())
{
  _getSystemTime = getSystemTime;
  timeFunctionRegistered = true;
}

/*
 * Lets the compiler/linker know what types of templates we are expecting to
 * have this class instantiated with.  Basically, it prepares the program to
 * make a PIDController of any of these defined types.
 */
template class PIDController<int>;
template class PIDController<long>;
template class PIDController<float>;
template class PIDController<double>;