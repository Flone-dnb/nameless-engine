#pragma once
/*!
This is a class description for doxygen.
*/
class Foo { // sample class to show testing (see tests folder)
public:
    /**
     * @brief This is a function.
     *
     * This is a details description of this function.
     *
     * @param iValue the value that you need to add
     * @warning for more commands see https://www.doxygen.nl/manual/commands.html
     */
    void addValue(int iValue) { iCurrentValue = iValue; }
    /**
     * @brief This is a function.
     *
     * All public functions should be documented otherwise doxygen throws error.
     *
     * @return all returns types and parameters also should be documented otherwise
     * doxygen throws error.
     */
    int getValue() const { return iCurrentValue; }

private:
    int someFunc() const { return iCurrentValue; }

    int iCurrentValue = 0;
};