/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDocument.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsHTMLAttributes.h"
#include "nsIDOMMutationEvent.h"

class nsHTMLUnknownElement : public nsGenericHTMLContainerElement,
                             public nsIDOMHTMLElement
{
public:
  nsHTMLUnknownElement();
  virtual ~nsHTMLUnknownElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsAString& aValue, PRBool aNotify);
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
};

nsresult
NS_NewHTMLUnknownElement(nsIHTMLContent** aInstancePtrResult,
                         nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLUnknownElement* it = new nsHTMLUnknownElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLUnknownElement::nsHTMLUnknownElement()
{
}

nsHTMLUnknownElement::~nsHTMLUnknownElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLUnknownElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLUnknownElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLUnknownElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLUnknownElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLUnknownElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLUnknownElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLUnknownElement* it = new nsHTMLUnknownElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet *sheet=nsnull;

  if (aDocument) {
    nsCOMPtr<nsIHTMLContentContainer> container(do_QueryInterface(aDocument));

    container->GetAttributeStyleSheet(&sheet);
  }

  return sheet;
}


NS_IMETHODIMP
nsHTMLUnknownElement::SetAttribute(PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   PRBool aNotify)
{
  nsresult  result = NS_OK;

  // Check for event handlers
  if (aNameSpaceID == kNameSpaceID_None && IsEventName(aAttribute)) {
    AddScriptEventListener(aAttribute, aValue);
  }
  
  nsHTMLValue val;
  
  if (NS_CONTENT_ATTR_NOT_THERE !=
      StringToAttribute(aAttribute, aValue, val)) {
    // string value was mapped to nsHTMLValue, set it that way
    result = SetHTMLAttribute(aAttribute, val, aNotify);

    return result;
  } else {
    if (ParseCommonAttribute(aAttribute, aValue, val)) {
      // string value was mapped to nsHTMLValue, set it that way
      result = SetHTMLAttribute(aAttribute, val, aNotify);
      return result;
    }

    if (aValue.IsEmpty()) { // if empty string
      val.SetEmptyValue();
      result = SetHTMLAttribute(aAttribute, val, aNotify);
      return result;
    }

    if (aNotify && (mDocument)) {
      mDocument->BeginUpdate();

      mDocument->AttributeWillChange(this, aNameSpaceID, aAttribute);
    }

    // set as string value to avoid another string copy
    nsChangeHint impact = NS_STYLE_HINT_NONE;
    GetMappedAttributeImpact(aAttribute, nsIDOMMutationEvent::MODIFICATION, impact);

    nsCOMPtr<nsIHTMLStyleSheet> sheet(dont_AddRef(GetAttrStyleSheet(mDocument)));
    if (!mAttributes) {
      result = NS_NewHTMLAttributes(&mAttributes);
      NS_ENSURE_SUCCESS(result, result);
    }
    result = mAttributes->SetAttributeFor(aAttribute, aValue, 
                                          (impact & ~(nsChangeHint_AttrChange | nsChangeHint_Aural
                                                      | nsChangeHint_Content)) != 0,
                                          this, sheet);
  }

  if (aNotify && (mDocument)) {
    result = mDocument->AttributeChanged(this, aNameSpaceID, aAttribute, nsIDOMMutationEvent::MODIFICATION, 
                                         NS_STYLE_HINT_UNKNOWN);
    mDocument->EndUpdate();
  }

  return result;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLUnknownElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
