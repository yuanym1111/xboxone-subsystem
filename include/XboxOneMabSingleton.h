/**
 * @file XboxOneMabSingleton.h
 * @Base singleton class.
 * @author jaydens
 * @22/10/2014
 */

#ifndef XBOX_ONE_MAB_SINGLETON_H
#define XBOX_ONE_MAB_SINGLETON_H

template <class T> class XboxOneMabSingleton
{
public:
	static bool Create();
	static void Destroy() { delete s_pInstance;}
	static bool IsCreated() { return s_pInstance != NULL;}
	static T* Get() { return s_pInstance;}

	static T* s_pInstance;
};

template <class T> T* XboxOneMabSingleton<T>::s_pInstance = NULL;

template <class T> bool XboxOneMabSingleton<T>::Create()
{
	if (IsCreated()) return true;

	s_pInstance = new T;
	return (s_pInstance != NULL);
}

#endif